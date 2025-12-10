/*
 * Runs BLE tasks and conversion tasks
 */
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
#include "ble.h"
#include "rssi_to_dist.h"
#include <stdio.h>

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

