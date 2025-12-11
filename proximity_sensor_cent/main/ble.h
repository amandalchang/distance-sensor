/*
 * BLE header file
 */

#ifndef BLE_H
#define BLE_H

#include <stdint.h>

/**
 * Run nimble host task.
 *
 * Create nimble host event loop to proccess BLE host events.
 * Once host loop ends, clean up nimble host resources and end task.
 */
void ble_prox_cent_host_task(void* param);

/**
 * Get the current RSSI reading.
 *
 * Read the RSSI value between the receiver and transmitter
 * devices.
 *
 * @return The RSSI value.
 */
int8_t get_rssi(void);

/**
 * Run BLE initialization routine.
 *
 * Initialize all required BLE initialization steps for proximity
 * application. Including: NVS flash storage, nimble host and
 * controller stack,\path loss task, configure BLE host callbacks,
 * data structures to trackconnect peers, and store host configuration.
 *
 * @return An int 0 representing successful initialization. Else,
 * returns 1 which means nimble failed to initialize.
 */
int initialize_ble(void);

#endif
