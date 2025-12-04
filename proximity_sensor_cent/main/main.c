/*
 * SPDX-FileCopyrightText: 2017-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "calculation.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include <math.h>
/* BLE */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "ble_prox_cent.h"

#define BUTTON_PIN 13
#define LED 12
#define CALIBRATED

#ifdef CALIBRATED
// manually measured approximate rssi to distance values
float m = -12;
float b = -35;
#else
// for converting rssi to distance with logreg
float m; // slope of linear regression
float b; // y intercept of linear regression
#endif

static const char *tag = "NimBLE_PROX_CENT";
static uint8_t link_supervision_timeout;
static int8_t tx_pwr_lvl;
static struct ble_prox_cent_conn_peer conn_peer[MYNEWT_VAL(BLE_MAX_CONNECTIONS) + 1];
static struct ble_prox_cent_link_lost_peer disconn_peer[MYNEWT_VAL(BLE_MAX_CONNECTIONS) + 1];



// Pin defs
#define GPIO_STCP (gpio_num_t) 27 // ST_CP (Storage Register Clock / Latch)
#define GPIO_SHCP (gpio_num_t) 26 // SH_CP (Shift Register Clock)
#define GPIO_DS   (gpio_num_t) 25 // DS (Data Input)

// hex representations of the numbers 0 through 9
const uint8_t datArray[] = {
    0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f
};

void shift_out_msb(gpio_num_t dataPin, gpio_num_t clockPin, uint8_t val) {
    for (int i = 0; i < 8; i++) {
        // Determine the most significant bit (MSB)
        uint8_t bit = (val & (0x80 >> i)); 

        // 1. Write the bit to the Data Pin
        gpio_set_level(dataPin, bit ? 1 : 0);

        // 2. Pulse the Clock Pin (SH_CP) to shift the data
        gpio_set_level(clockPin, 1);
        gpio_set_level(clockPin, 0);
    }
}

// Function to set up the GPIO pins
void setup_gpio() {
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);

    gpio_set_direction(GPIO_STCP, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_SHCP, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_DS,   GPIO_MODE_OUTPUT);
    
    // Set initial levels
    gpio_set_level(GPIO_STCP, 0);
    gpio_set_level(GPIO_SHCP, 0);
    gpio_set_level(GPIO_DS, 0);
}

void ble_store_config_init(void);
static void ble_prox_cent_scan(void);
static int ble_prox_cent_gap_event(struct ble_gap_event *event, void *arg);

static int
ble_prox_cent_on_read(uint16_t conn_handle,
                      const struct ble_gatt_error *error,
                      struct ble_gatt_attr *attr,
                      void *arg)
{
    MODLOG_DFLT(INFO, "Read on tx power level char completed; status=%d "
                "conn_handle=%d\n",
                error->status, conn_handle);
    if (error->status == 0) {
        MODLOG_DFLT(INFO, " attr_handle=%d value=", attr->handle);
        print_mbuf(attr->om);
        os_mbuf_copydata(attr->om, 0, attr->om->om_len, &tx_pwr_lvl);
        conn_peer[conn_handle].calc_path_loss = true;
    }

    return 0;
}

/**
 * Application callback.  Called when the write of alert level char
 * characteristic has completed.
 */
static int
ble_prox_cent_on_write(uint16_t conn_handle,
                       const struct ble_gatt_error *error,
                       struct ble_gatt_attr *attr,
                       void *arg)
{
    MODLOG_DFLT(INFO, "Write alert level char completed; status=%d conn_handle=%d",
                error->status, conn_handle);

    /* Read Tx Power level characteristic. */
    const struct peer_chr *chr;
    int rc;
    const struct peer *peer = peer_find(conn_handle);

    chr = peer_chr_find_uuid(peer,
                             BLE_UUID16_DECLARE(BLE_SVC_TX_POWER_UUID16),
                             BLE_UUID16_DECLARE(BLE_SVC_PROX_CHR_UUID16_TX_PWR_LVL));
    if (chr == NULL) {
        MODLOG_DFLT(ERROR, "Error: Peer doesn't support the"
                    "Tx power level characteristic\n");
        goto err;
    }

    rc = ble_gattc_read(conn_handle, chr->chr.val_handle,
                        ble_prox_cent_on_read, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error: Failed to read characteristic; rc=%d\n",
                    rc);
        goto err;
    }

    return 0;
err:
    /* Terminate the connection. */
    return ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

/**
 * Performs following GATT operations against the specified peer:
 * 1. Writes the alert level characteristic.
 * 2. After write is completed, it reads the Tx Power Level characteristic.
 *
 * If the peer does not support a required service, characteristic, or
 * descriptor, then the peer lied when it claimed support for the link
 * loss service!  When this happens, or if a GATT procedure fails,
 * this function immediately terminates the connection.
 */
static void
ble_prox_cent_read_write_subscribe(const struct peer *peer)
{
    const struct peer_chr *chr;
    int rc;

    /* Storing the val handle of immediate alert characteristic */
    chr = peer_chr_find_uuid(peer,
                             BLE_UUID16_DECLARE(BLE_SVC_IMMEDIATE_ALERT_UUID16),
                             BLE_UUID16_DECLARE(BLE_SVC_PROX_CHR_UUID16_ALERT_LVL));
    if (chr != NULL) {
        conn_peer[peer->conn_handle].val_handle = chr->chr.val_handle;
    } else {
        MODLOG_DFLT(ERROR, "Error: Peer doesn't support the alert level"
                    " characteristic of immediate alert loss service\n");
    }

    /* Write alert level characteristic. */
    chr = peer_chr_find_uuid(peer,
                             BLE_UUID16_DECLARE(BLE_SVC_LINK_LOSS_UUID16),
                             BLE_UUID16_DECLARE(BLE_SVC_PROX_CHR_UUID16_ALERT_LVL));
    if (chr == NULL) {
        MODLOG_DFLT(ERROR, "Error: Peer doesn't support the alert level"
                    " characteristic\n");
        goto err;
    }

    rc = ble_gattc_write_flat(peer->conn_handle, chr->chr.val_handle,
                              &link_supervision_timeout, sizeof(link_supervision_timeout),
                              ble_prox_cent_on_write, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error: Failed to write characteristic; rc=%d\n",
                    rc);
        goto err;
    }

    return;
err:
    /* Terminate the connection. */
    ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

/**
 * Called when service discovery of the specified peer has completed.
 */
static void
ble_prox_cent_on_disc_complete(const struct peer *peer, int status, void *arg)
{

    if (status != 0) {
        /* Service discovery failed.  Terminate the connection. */
        MODLOG_DFLT(ERROR, "Error: Service discovery failed; status=%d "
                    "conn_handle=%d\n", status, peer->conn_handle);
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    /* Service discovery has completed successfully.  Now we have a complete
     * list of services, characteristics, and descriptors that the peer
     * supports.
     */
    MODLOG_DFLT(INFO, "Service discovery complete; status=%d "
                "conn_handle=%d\n", status, peer->conn_handle);

    /* Now perform GATT procedures against the peer: read,
     * write.
     */
    ble_prox_cent_read_write_subscribe(peer);
}

/**
 * Initiates the GAP general discovery procedure.
 */
static void
ble_prox_cent_scan(void)
{
    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params = {0};
    int rc;

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }

    /* Tell the controller to filter duplicates; we don't want to process
     * repeated advertisements from the same device.
     */
    disc_params.filter_duplicates = 1;

    /**
     * Perform a passive scan.  I.e., don't send follow-up scan requests to
     * each advertiser.
     */
    disc_params.passive = 1;

    /* Use defaults for the rest of the parameters. */
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params,
                      ble_prox_cent_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error initiating GAP discovery procedure; rc=%d\n",
                    rc);
    }
}

/**
 * Indicates whether we should try to connect to the sender of the specified
 * advertisement.  The function returns a positive result if the device
 * advertises connectability and support for the Health Thermometer service.
 */
static int
ble_prox_cent_should_connect(const struct ble_gap_disc_desc *disc)
{
    struct ble_hs_adv_fields fields;
    int rc;
    int i;
    uint8_t test_addr[6];
    uint32_t peer_addr[6];

    memset(peer_addr, 0x0, sizeof peer_addr);

    /* The device has to be advertising connectability. */
    if (disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND &&
            disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND) {

        return 0;
    }

    rc = ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data);
    if (rc != 0) {
        return rc;
    }

    if (strlen(CONFIG_EXAMPLE_PEER_ADDR) && (strncmp(CONFIG_EXAMPLE_PEER_ADDR, "ADDR_ANY", strlen("ADDR_ANY")) != 0)) {
        ESP_LOGI(tag, "Peer address from menuconfig: %s", CONFIG_EXAMPLE_PEER_ADDR);
        /* Convert string to address */
        sscanf(CONFIG_EXAMPLE_PEER_ADDR, "%lx:%lx:%lx:%lx:%lx:%lx",
               &peer_addr[5], &peer_addr[4], &peer_addr[3],
               &peer_addr[2], &peer_addr[1], &peer_addr[0]);

	/* Conversion */
        for (int i=0; i<6; i++) {
            test_addr[i] = (uint8_t )peer_addr[i];
        }

        if (memcmp(test_addr, disc->addr.val, sizeof(disc->addr.val)) != 0) {
            return 0;
        }
    }

    /* The device has to advertise support for the Proximity sensor (link loss)
     * service (0x1803).
     */
    for (i = 0; i < fields.num_uuids16; i++) {
        if (ble_uuid_u16(&fields.uuids16[i].u) == BLE_SVC_LINK_LOSS_UUID16) {
            return 1;
        }
    }

    return 0;
}

/**
 * Connects to the sender of the specified advertisement of it looks
 * interesting.  A device is "interesting" if it advertises connectability and
 * support for the Proximity Sensor service.
 */
static void
ble_prox_cent_connect_if_interesting(void *disc)
{
    uint8_t own_addr_type;
    int rc;
    ble_addr_t *addr;

    /* Don't do anything if we don't care about this advertiser. */
if (!ble_prox_cent_should_connect((struct ble_gap_disc_desc *)disc)) {
        return;
    }

#if !(MYNEWT_VAL(BLE_HOST_ALLOW_CONNECT_WITH_SCAN))
    /* Scanning must be stopped before a connection can be initiated. */
    rc = ble_gap_disc_cancel();
    if (rc != 0) {
        MODLOG_DFLT(DEBUG, "Failed to cancel scan; rc=%d\n", rc);
        return;
    }
#endif

    /* Figure out address to use for connect (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }

    /* Try to connect the the advertiser.  Allow 30 seconds (30000 ms) for
     * timeout.
     */
#if CONFIG_EXAMPLE_EXTENDED_ADV
    addr = &((struct ble_gap_ext_disc_desc *)disc)->addr;
#else
    addr = &((struct ble_gap_disc_desc *)disc)->addr;
#endif
    rc = ble_gap_connect(own_addr_type, addr, 30000, NULL,
                         ble_prox_cent_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error: Failed to connect to device; addr_type=%d "
                    "addr=%s; rc=%d\n",
                    addr->type, addr_str(addr->val), rc);
        return;
    }
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that is
 * established.  ble_prox_cent uses the same callback for all connections.
 *
 * @param event                 The event being signalled.
 * @param arg                   Application-specified argument; unused by
 *                              ble_prox_cent.
 *
 * @return                      0 if the application successfully handled the
 *                              event; nonzero on failure.  The semantics
 *                              of the return code is specific to the
 *                              particular GAP event being signalled.
 */
static int
ble_prox_cent_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    struct ble_hs_adv_fields fields;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        rc = ble_hs_adv_parse_fields(&fields, event->disc.data,
                                     event->disc.length_data);
        if (rc != 0) {
            return 0;
        }

        /* An advertisement report was received during GAP discovery. */
        print_adv_fields(&fields);

        /* Try to connect to the advertiser if it looks interesting. */
        ble_prox_cent_connect_if_interesting(&event->disc);
        return 0;

    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        if (event->connect.status == 0) {
            /* Connection successfully established. */
            MODLOG_DFLT(INFO, "Connection established ");

            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            print_conn_desc(&desc);
            MODLOG_DFLT(INFO, "\n");

            link_supervision_timeout = 8 * desc.conn_itvl;

            /* Remember peer. */
            rc = peer_add(event->connect.conn_handle);
            if (rc != 0) {
                MODLOG_DFLT(ERROR, "Failed to add peer; rc=%d\n", rc);
                return 0;
            }

            /* Check if this device is reconnected */
            for (int i = 0; i <= MYNEWT_VAL(BLE_MAX_CONNECTIONS); i++) {
                if (disconn_peer[i].addr != NULL) {
                    if (memcmp(disconn_peer[i].addr, &desc.peer_id_addr.val, BLE_ADDR_LEN)) {
                        /* Peer reconnected. Stop alert for this peer */
                        free(disconn_peer[i].addr);
                        disconn_peer[i].addr = NULL;
                        disconn_peer[i].link_lost = false;
                        break;
                    }
                }
            }

#if MYNEWT_VAL(BLE_GATT_CACHING_ASSOC_ENABLE)
            rc =  ble_gattc_cache_assoc(desc.peer_id_addr);
            if (rc != 0) {
                MODLOG_DFLT(ERROR, "Cache Association Failed; rc=%d\n", rc);
                return 0;
            }
#else
            /* Perform service discovery */
            rc = peer_disc_all(event->connect.conn_handle,
                               ble_prox_cent_on_disc_complete, NULL);
            if (rc != 0) {
                MODLOG_DFLT(ERROR, "Failed to discover services; rc=%d\n", rc);
                return 0;
            }
#endif // BLE_GATT_CACHING_ASSOC_ENABLE
        } else {
            /* Connection attempt failed; resume scanning. */
            MODLOG_DFLT(ERROR, "Error: Connection failed; status=%d\n",
                        event->connect.status);
        }
        ble_prox_cent_scan();
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        /* Connection terminated. */
        MODLOG_DFLT(INFO, "disconnect; reason=%d ", event->disconnect.reason);
        print_conn_desc(&event->disconnect.conn);
        MODLOG_DFLT(INFO, "\n");

        /* Start the link loss alert for this connection handle */
        for (int i = 0; i <= MYNEWT_VAL(BLE_MAX_CONNECTIONS); i++) {
            if (disconn_peer[i].addr == NULL) {
                disconn_peer[i].addr = (uint8_t *)malloc(BLE_ADDR_LEN * sizeof(uint8_t));
                if (disconn_peer[i].addr == NULL) {
                    return BLE_HS_ENOMEM;
                }
                memcpy(disconn_peer[i].addr, &event->disconnect.conn.peer_id_addr.val,
                       BLE_ADDR_LEN);
                disconn_peer[i].link_lost = true;
                break;
            }
        }
        /* Stop calculating path loss, restart once connection is established again */
        conn_peer[event->disconnect.conn.conn_handle].calc_path_loss = false;
        conn_peer[event->disconnect.conn.conn_handle].val_handle = 0;

        /* Forget about peer. */
        peer_delete(event->disconnect.conn.conn_handle);

        /* Resume scanning. */
        ble_prox_cent_scan();
        return 0;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        MODLOG_DFLT(INFO, "discovery complete; reason=%d\n",
                    event->disc_complete.reason);
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption has been enabled or disabled for this connection. */
        MODLOG_DFLT(INFO, "encryption change event; status=%d ",
                    event->enc_change.status);
        rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        assert(rc == 0);
        print_conn_desc(&desc);
#if CONFIG_EXAMPLE_ENCRYPTION
#if MYNEWT_VAL(BLE_GATT_CACHING_ASSOC_ENABLE)
        rc =  ble_gattc_cache_assoc(desc.peer_id_addr);
        if (rc != 0) {
            MODLOG_DFLT(ERROR, "Cache Association Failed; rc=%d\n", rc);
            return 0;
        }
#else
        /*** Go for service discovery after encryption has been successfully enabled ***/
        rc = peer_disc_all(event->connect.conn_handle,
                           ble_prox_cent_on_disc_complete, NULL);
        if (rc != 0) {
            MODLOG_DFLT(ERROR, "Failed to discover services; rc=%d\n", rc);
            return 0;
        }
#endif // BLE_GATT_CACHING_ASSOC_ENABLE
#endif
        return 0;

//     case BLE_GAP_EVENT_CACHE_ASSOC:
// #if MYNEWT_VAL(BLE_GATT_CACHING_ASSOC_ENABLE)
//           /* Cache association result for this connection */
//           MODLOG_DFLT(INFO, "cache association; conn_handle=%d status=%d cache_state=%s\n",
//                       event->cache_assoc.conn_handle,
//                       event->cache_assoc.status,
//                       (event->cache_assoc.cache_state == 0) ? "INVALID" : "LOADED");
//           /* Perform service discovery */
//           rc = peer_disc_all(event->connect.conn_handle,
//                              blecent_on_disc_complete, NULL);
//           if(rc != 0) {
//                 MODLOG_DFLT(ERROR, "Failed to discover services; rc=%d\n", rc);
//                 return 0;
//           }
// #endif
        //   return 0;

    case BLE_GAP_EVENT_NOTIFY_RX:
        /* Peer sent us a notification or indication. */
        MODLOG_DFLT(INFO, "received %s; conn_handle=%d attr_handle=%d "
                    "attr_len=%d\n",
                    event->notify_rx.indication ?
                    "indication" :
                    "notification",
                    event->notify_rx.conn_handle,
                    event->notify_rx.attr_handle,
                    OS_MBUF_PKTLEN(event->notify_rx.om));

        /* Attribute data is contained in event->notify_rx.om. Use
         * `os_mbuf_copydata` to copy the data received in notification mbuf */
        return 0;

    case BLE_GAP_EVENT_MTU:
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* We already have a bond with the peer, but it is attempting to
         * establish a new secure link.  This app sacrifices security for
         * convenience: just throw away the old bond and accept the new link.
         */

        /* Delete the old bond. */
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        assert(rc == 0);
        ble_store_util_delete_peer(&desc.peer_id_addr);

        /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
         * continue with the pairing operation.
         */
        return BLE_GAP_REPEAT_PAIRING_RETRY;

#if CONFIG_EXAMPLE_EXTENDED_ADV
    case BLE_GAP_EVENT_EXT_DISC:
        /* An advertisement report was received during GAP discovery. */
        ext_print_adv_report(&event->ext_disc);

        ble_prox_cent_connect_if_interesting(&event->ext_disc);
        return 0;
#endif

    default:
        return 0;
    }
}

// void
// ble_prox_cent_path_loss_task(void *pvParameters)
// {
//     int8_t rssi;
//     int rc;
//     float dist;

//     while (1) {
//         for (int i = 0; i <= MYNEWT_VAL(BLE_MAX_CONNECTIONS); i++) {
//             if (conn_peer[i].calc_path_loss) {
//                 // MODLOG_DFLT(INFO, "Connection handle : %d", i);
//                 rc = ble_gap_conn_rssi(i, &rssi);
//                 if (rc == 0) {
//                     MODLOG_DFLT(INFO, "Current RSSI = %d", rssi);
//                 } else {
//                     MODLOG_DFLT(ERROR, "Failed to get current RSSI");
//                 }
//             }
//         }
//         vTaskDelay(1000 / portTICK_PERIOD_MS);
//     }
// }

void
ble_prox_cent_link_loss_task(void *pvParameters)
{
    while (1) {
        for (int i = 0; i <= MYNEWT_VAL(BLE_MAX_CONNECTIONS); i++) {
            if (disconn_peer[i].link_lost && disconn_peer[i].addr != NULL) {
                MODLOG_DFLT(INFO, "Link lost for device with conn_handle %d", i);
            }
        }
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

static void
ble_prox_cent_on_reset(int reason)
{
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void
ble_prox_cent_on_sync(void)
{
    int rc;

    /* Make sure we have proper identity address set (public preferred) */
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    /* Begin scanning for a peripheral to connect to. */
    ble_prox_cent_scan();
}

void ble_prox_cent_host_task(void *param)
{
    ESP_LOGI(tag, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

int8_t get_rssi(void) {
  int8_t rssi = 0;

  for (int i = 0; i <= MYNEWT_VAL(BLE_MAX_CONNECTIONS); i++) {
    if (conn_peer[i].calc_path_loss) {
      ble_gap_conn_rssi(i, &rssi);
    }
  }
  return rssi;
}

void distance_conversion_task(void *pvParameters) {
    int collected_rssi_size = 10;
    MODLOG_DFLT(INFO, "Distance Conversion Beginning");

    int8_t collected_rssi[collected_rssi_size];
    float dist = 0;
    float sum = 0.0f;
    while (true) {
        // display 9 on the 7 seg display 
        gpio_set_level(GPIO_STCP, 0); 
        shift_out_msb(GPIO_DS, GPIO_SHCP, 0x39); // show C when calibrated
        gpio_set_level(GPIO_STCP, 1); 
        vTaskDelay(1);  // yields to Idle task and resets watchdog
        // on btn click
        sum = 0.0f;
        for (int j = 0; j < collected_rssi_size; j++) {
        // get rssi value
        collected_rssi[j] = get_rssi();
        sum += collected_rssi[j];
        vTaskDelay(pdMS_TO_TICKS(200)); // Delay 0.25 sec
        }
        if (sum != 0.0f) {
        MODLOG_DFLT(INFO, "%f, %f, %f", (sum/collected_rssi_size), m, b);
        // calibrated every 10 inches
        dist = rssi_to_dist((sum/collected_rssi_size), m, b);
        MODLOG_DFLT(INFO, "Distance (in) = %.2f", dist);
        } else {
            // check if there aren't any values in rssi
            MODLOG_DFLT(DEBUG, "No RSSI values detected");
            }
        }
}

void calibration_task(void *pvParameters) {
    gpio_reset_pin(BUTTON_PIN);
    gpio_reset_pin(LED);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);

    int array_size = 5;
    int averaging_array_size = 30;

    int8_t temp_cali_location_arr[averaging_array_size];
    float rssi_array[array_size];
    int i = 0;
    float sum = 0.0f;

    while (i < array_size) {
                // display round number on the 7 seg display 
        gpio_set_level(GPIO_STCP, 0); 
        shift_out_msb(GPIO_DS, GPIO_SHCP, datArray[i+1]);
        gpio_set_level(GPIO_STCP, 1); 
        gpio_set_level(LED, 1); // light on indicate move cali location
        vTaskDelay(1);  // yields to Idle task and resets watchdog
      // on btn click
        if (gpio_get_level(BUTTON_PIN)) {
            gpio_set_level(LED, 0); // light off indicate calibration started
            sum = 0.0f;
            for (int j = 0; j < averaging_array_size; j++) {
            // get rssi value
            temp_cali_location_arr[j] = get_rssi();
            //   temp_cali_location_arr[j] = 2.0f;
            MODLOG_DFLT(INFO, "Current RSSI = %d", temp_cali_location_arr[j]);
            sum += temp_cali_location_arr[j];
                vTaskDelay(pdMS_TO_TICKS(250)); // Delay 0.25 sec
            }
            if (sum != 0.0f) {
            rssi_array[i] = sum / averaging_array_size;
            } else {
            // check if there aren't any values in rssi
            rssi_array[i] = -1.0f;
            }
            MODLOG_DFLT(INFO, "Calibration mean[%d] = %.2f", i,
                        rssi_array[i]); 
            i++;
      }
    }
    rssi_logreg_to_params(array_size, rssi_array, &m, &b);
    xTaskCreate(distance_conversion_task, "distance_conversion_task", 4096, NULL, 10, NULL);
    vTaskDelete(NULL);
}

static void
ble_prox_cent_init(void)
{
    // /* Task for calculating path loss */
    // xTaskCreate(ble_prox_cent_path_loss_task, "ble_prox_cent_path_loss_task", 4096, NULL, 10, NULL);

    /* Task for alerting when link is lost */
    xTaskCreate(ble_prox_cent_link_loss_task, "ble_prox_cent_link_loss_task", 4096, NULL, 10, NULL);

    #ifdef CALIBRATED
        xTaskCreate(distance_conversion_task, "distance_conversion_task", 4096, NULL, 10, NULL);
    #else
        xTaskCreate(calibration_task, "calibration_task", 4096, NULL, 10, NULL);
    #endif
    return;
}

void
app_main(void)
{
    int rc;
    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if  (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(tag, "Failed to init nimble %d ", ret);
        return;
    }

    /* Initialize a task to keep checking path loss of the link */
    setup_gpio();
    ble_prox_cent_init();

    for (int i = 0; i <= MYNEWT_VAL(BLE_MAX_CONNECTIONS); i++) {
        disconn_peer[i].addr = NULL;
        disconn_peer[i].link_lost = true;
    }

    /* Configure the host. */
    ble_hs_cfg.reset_cb = ble_prox_cent_on_reset;
    ble_hs_cfg.sync_cb = ble_prox_cent_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Initialize data structures to track connected peers. */
    rc = peer_init(MYNEWT_VAL(BLE_MAX_CONNECTIONS), 64, 64, 64);
    assert(rc == 0);

    /* XXX Need to have template for store */
    ble_store_config_init();

    nimble_port_freertos_init(ble_prox_cent_host_task);
}
