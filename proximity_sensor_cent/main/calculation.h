/*
 * Calculation header file for import into main
 */

#ifndef CALCULATION_H
#define CALCULATION_H


/*
Takes an averaged rssi value as a float, a float slope of m and a float 
y-intercept of b and returns the 
*/
float rssi_to_dist(const float rssi, const float m, const float b);
void rssi_logreg_to_params(const int num_dists, const float rssi_array[], float* m, float* b);

#endif
