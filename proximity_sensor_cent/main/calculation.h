/*
 * Calculation header file for import into main
 */

#ifndef CALCULATION_H
#define CALCULATION_H

/**
 * Compute the distance in meters related to RSSI value.
 *
 * Given an averaged rssi value, and the slope and the y-intercept
 * from calibration stage, use linear regression to find the distance
 * in meters that correspond to the RSSI value. Store distance to address
 * of dist param.
 *
 * @param dist The pointer to a float variable.
 * @param rssi The float of an RSSI value averaged.
 * @param m The float slope of the linear regression model.
 * @param b The float y-intercept of the linear regression model.
 *
 * @return The int 0 if conversion was successful, int 1 if not.
 */
int rssi_to_dist(float* dist, const float rssi, const float m, const float b);

/**
 * Turn RSSI calibration values into linear regression parameters.
 *
 * Given the number of calibration locations, the average RSSI value at
 * those calibration locations, calculate parameters for a linear
 * regression model and save the slope and y-intercept to given addresses
 * m and b respectively.
 *
 * @param num_dists The int number of distances calibrated at.
 * @param rssi_array The float array with elements representing
 *  averaged RSSI values at each calibration location.
 * @param m The pointer to a float variable.
 * @param b The pointer to a float variable.
 */
void rssi_logreg_to_params(const int num_dists, const float* rssi_array,
                           float* m, float* b);

#endif
