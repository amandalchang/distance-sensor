/**
 * Runs BLE tasks and conversion tasks
 */

#include <stdio.h>

#include "ble.h"
#include "nimble/nimble_port_freertos.h"
#include "rssi_to_dist.h"

void app_main(void) {
  int rc;

  rc = initialize_ble();
  if (rc == 0) {
    nimble_port_freertos_init(ble_prox_cent_host_task);
    xTaskCreate(distance_conversion_task, "distance_conversion_task", 4096,
                NULL, 11, NULL);
  }
}
