/*
 * RSSI header file for averaging and returning distances
 */

#ifndef RSSI_TO_DIST
#define RSSI_TO_DIST

/**
 * Task for distance-measurement params and starting distance
 * averaging task.
 * 
 * Initializes GPIO pins for hardware, starts task for calibration if not
 * calibration is needed, then begins distance averaging task.
 * 
 */
void distance_conversion_task(void *pvParameters);

#endif