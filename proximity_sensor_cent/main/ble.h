/*
 * BLE header file
 */

#ifndef BLE_H
#define BLE_H


int8_t get_rssi(void);
void ble_prox_cent_link_loss_task(void *pvParameters)
void ble_prox_cent_host_task(void *param);

#endif
