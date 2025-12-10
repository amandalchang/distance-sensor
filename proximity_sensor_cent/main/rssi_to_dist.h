/*
 * RSSI header file for averaging and returning distances
 */

#ifndef RSSI_TO_DIST
#define RSSI_TO_DIST


float calibration_task(void);
float distance_conversion_task(void);
void rssi_logreg_to_params(const int num_dists, const float rssi_array[], float* m, float* b);

#endif