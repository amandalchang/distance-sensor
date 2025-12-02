//
// Convert cleaned RSSI value to distance in meters using logarithmic regression
//
#include "calculation.h"
#include <math.h>
#include <stdio.h>

// avoids using pow(x, 2) which is less clean and slower
static inline float sqr(float d) { return d*d; }

// Does linear regression using least squares
int linreg(int num_dists, const float x[], const float rssi_array[], float* m, float* b, float* r) {
    float sumx = 0.0;
    float sumx2 = 0.0;
    float sumxy = 0.0;
    float sumy = 0.0;
    float sumy2 = 0.0;

    for (int i = 0; i < num_dists; i++) {
        sumx  += x[i];
        sumx2 += sqr(x[i]);
        sumxy += x[i] * rssi_array[i];
        sumy  += rssi_array[i];
        sumy2 += sqr(rssi_array[i]);
    }

    float denom = (num_dists * sumx2 - sqr(sumx));
    if (denom == 0.0) {
        *m = 0.0;
        *b = 0.0;
        if (r) *r = 0.0;
        return 1;
    }

    *m = (num_dists * sumxy - sumx * sumy) / denom;
    *b = (sumy * sumx2 - sumx * sumxy) / denom;

    if (r != NULL) {
        *r = (sumxy - sumx * sumy / num_dists) /
             sqrt((sumx2 - sqr(sumx) / num_dists) *
                  (sumy2 - sqr(sumy) / num_dists));
    }

    return 0;
}

// validatation of logarithmic regression model
float rssi_to_dist(const float rssi, const float m, const float b) {
    return pow(10, (rssi - b) / m);
}

void rssi_logreg_to_params(const int num_dists, const float rssi_array[], float* m, float* b) {
    float log_distances[num_dists]; // storage for log distances
    float r = 0;
    
    // assuming 1 meter apart from each measurement
    for (int i = 0; i < num_dists; i+=1) {
        log_distances[i] = log10(i+1);
        printf("log %i is %.3f\n", i+1, log_distances[i]);
        printf("for the %ith element, rssi = %f\n", (i+1), rssi_array[i]);
    };
    linreg(num_dists, log_distances, rssi_array, m, b, &r);
    printf("rssi = %.3f + %.3f * log_dists\nr = %.3f\n", *b, *m, r);
}


// int main(void) {
//     const int num_dists = 5; // number of measurements
//     float m = 0; // stores slope
//     float b = 0; // stores y-intercept
//     float r = 0;
//     float distances[num_dists]; // fills in calculated distances

//     // initializing sample rssi values
//     // const float rssi_array[5] = {-5.000, 5.602, 5.954, 6.204, 6.398};
//     // const float rssi_array[5] = {-2.0, -3.51, -4.39, -5.01, -5.49};
//     const float rssi_array[5] = {-56.5, -61.08, -57.9, -61.58, -58.25};
//     rssi_logreg_to_params(num_dists, rssi_array, &m, &b);

//     // convert rssi to distances using calculated equation
//     for (int i = 0; i < num_dists; i+=1) {
//         float distance = rssi_to_dist(rssi_array[i], m, b);
//         printf("The original distance was %i and the calculated is %0.4f\n", i+1, distance);
//     };

//     // save calculated logarithmic parameters
//     FILE *f = fopen("rssi_params.txt", "w");
//     fprintf(f, "%0.3f %0.3f\n", m, b);
//     fclose(f);
// }


// /*
// Calibration file for RSSI to meters
// */

// #include "driver/gpio.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "iot_button.h"
// #include "sdkconfig.h"
// #include <stdio.h>

// // define pins
// #define BUTTON_PIN 4
// #define LED 5

// // https://randomnerdtutorials.com/esp-idf-esp32-gpio-inputs/

// // gpio_config_t io_conf = {
// //     .pin_bit_mask = (1ULL << BUTTON_PIN),   // Select GPIO 4
// //     .mode = GPIO_MODE_INPUT,                  // Set as input
// //     .pull_up_en = GPIO_PULLUP_ENABLE,     // Enable internal pull-up
// //     .pull_down_en = GPIO_PULLDOWN_DISABLE, // Disable pull-down
// //     .intr_type = GPIO_INTR_DISABLE        // Disable interrupts
// // };
// // gpio_config(&io_conf);

// int main(void) {
//     // configure
//     gpio_reset_pin(BUTTON_PIN);
//     gpio_reset_pin(LED);
//     gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
//     gpio_set_direction(LED, GPIO_MODE_OUTPUT);

//     if (gpio_get_level(BUTTON_PIN)) {
//       // start 10
//       // record all rssi measurements to an array of fixed size 10
//     }
//     // non-blocking send the average out to a different variable
    

//   while (1) {
//     if (gpio_get_level(BUTTON_PIN)) {
//         gpio_set_level(LED, 1);
//     } else {
//         gpio_set_level(LED, 0);
//     }
//   }
// }