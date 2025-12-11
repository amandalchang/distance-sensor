/**
 * Convert cleaned RSSI value to distance in meters using
 * logarithmic regression.
 */

#include "calculation.h"

#include <math.h>
#include <stdio.h>

// avoids using pow(x, 2) which is less clean and slower
static inline float sqr(float digit) { return digit * digit; }

// Does linear regression using least squares
int linreg(int num_dists, const float x[], const float rssi_array[],
           float* m_val, float* b_val, float* r_val) {
  float sumx = 0.0F;
  float sumx2 = 0.0F;
  float sumxy = 0.0F;
  float sumy = 0.0F;
  float sumy2 = 0.0F;

  for (int i = 0; i < num_dists; i++) {
    sumx += x[i];
    sumx2 += sqr(x[i]);
    sumxy += x[i] * rssi_array[i];
    sumy += rssi_array[i];
    sumy2 += sqr(rssi_array[i]);
  }

  float denom = (((float)num_dists * sumx2) - sqr(sumx));
  if (denom == 0.0F) {
    *m_val = 0.0F;
    *b_val = 0.0F;
    if (r_val) {
      *r_val = 0.0F;
    }
    return 1;
  }

  *m_val = ((float)num_dists * sumxy - sumx * sumy) / denom;
  *b_val = (sumy * sumx2 - sumx * sumxy) / denom;

  if (r_val != NULL) {
    *r_val = (sumxy - sumx * sumy / (float)num_dists) /
             sqrtf((sumx2 - sqr(sumx) / (float)num_dists) *
                   (sumy2 - sqr(sumy) / (float)num_dists));
  }

  return 0;
}

// validatation of logarithmic regression model
int rssi_to_dist(float* dist, const float rssi, const float m_val,
                 const float b_val) {
  if (m_val != 0) {
    *dist = (float)pow(10, (rssi - b_val) / m_val);
    return 0;
  }
  return 1;
}

void rssi_logreg_to_params(const int num_dists, const float rssi_array[],
                           float* m_val, float* b_val) {
  float log_distances[num_dists];  // storage for log distances
  float r_val = 0;

  // 10 inches between each calibration unit, eventual distance in inches
  for (int i = 0; i < num_dists; i += 1) {
    log_distances[i] = (float)log10(i + ((i + 1) * 10));
    // printf("log %i is %.3f\n", i+1, log_distances[i]);
    // printf("for the %ith element, rssi = %f\n", (i+1), rssi_array[i]);
  };
  linreg(num_dists, log_distances, rssi_array, m_val, b_val, &r_val);
  // printf("rssi = %.3f + %.3f * log_dists\nr = %.3f\n", *b_val, *m_val,
  // r_val);
}

// int main(void) {
//     const int num_dists = 10; // number of measurements
//     float m = 0; // stores slope
//     float b = 0; // stores y-intercept

//     const float rssi_array[10] = {-46.766666, -51.566666, -53.033333,
//     -55.799999, -54.233334, -56.099998, -55.933334, -60.833332, -60.833332,
//     -60.066666}; rssi_logreg_to_params(num_dists, rssi_array, &m, &b);
//     printf("m = %f, b = %f\n", m, b);

//     // convert rssi to distances using calculated equation
//     float distance;
//     for (int i = 0; i < num_dists; i+=1) {
//         distance = rssi_to_dist(rssi_array[i], m, b);
//         printf("The original distance was %i and the calculated is %0.4f\n",
//         i+(i+1)*10, distance);
//     };

//     // save calculated logarithmic parameters
//     FILE *f = fopen("rssi_params.txt", "w");
//     fprintf(f, "%0.3f %0.3f\n", m, b);
//     fclose(f);
// }