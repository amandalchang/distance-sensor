/*
Calibration file for RSSI to meters
*/

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <stdio.h>

// define pins
#define BUTTON_PIN 34
#define LED 12

void calibration_task(void *pvParameters) {
    gpio_reset_pin(BUTTON_PIN);
    gpio_reset_pin(LED);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);

    int8_t temp_cali_location_arr[12];
    float cali_location_mean_arr[10];

    while (1) {
    int i = 0;
    float sum = 0.0f;

    while (i < 10) {
      gpio_set_level(LED, 1); // light on indicate move cali location
      // on btn click
      if (gpio_get_level(BUTTON_PIN)) {
        gpio_set_level(LED, 0); // light off indicate calibration started
        sum = 0.0f;
        for (int j = 0; j < 12; j++) {
          // get rssi value
          temp_cali_location_arr[j] = get_rssi();
        //   temp_cali_location_arr[j] = 2.0f;
          MODLOG_DFLT(INFO, "Current RSSI = %d", temp_cali_location_arr[j]);
          sum += temp_cali_location_arr[j];
          vTaskDelay(pdMS_TO_TICKS(500)); // Delay 0.5 sec
        }
        if (sum != 0.0f) {
          cali_location_mean_arr[i] = sum / 12.0f;
          ;
        } else {
          cali_location_mean_arr[i] = -1.0f;
        }
        MODLOG_DFLT(INFO, "Calibration mean[%d] = %.2f", i,
                    cali_location_mean_arr[i]);
        i++;
      }
    }
  }
  vTaskDelete(NULL);
}

// https://randomnerdtutorials.com/esp-idf-esp32-gpio-inputs/

// int temp_cali_location_arr[12];
// float cali_location_mean_arr[10];

// float get_cali_location_mean_val(void) {}

// int app_main(void) {
//   // configure
//   gpio_reset_pin(BUTTON_PIN);
//   gpio_reset_pin(LED);
//   gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
//   gpio_set_direction(LED, GPIO_MODE_OUTPUT);

//   while (1) {
//     int i = 0;
//     int sum = 0;
//     int avg = -1;

//     while (i < 10) {
//       gpio_set_level(LED, 1); // light on indicate move cali location
//       // on btn click
//       if (gpio_get_level(BUTTON_PIN)) {
//         gpio_set_level(LED, 0); // light off indicate calibration started
//         for (int j; j < 12; j++) {
//           // get rssi value
//           temp_cali_location_arr[j] = RSSI_HERE;
//           sum += temp_cali_location_arr[j];
//           vTaskDelay(pdMS_TO_TICKS(500)); // Delay 0.5 sec
//         }
//         avg = sum / 12;
//         cali_location_mean_arr[i] = avg;
//         i++;
//       }
//     }

//     // int curr_button_state = gpio_get_level(calibration_btn);
//     // if (gpio_get_level(BUTTON_PIN)) {
//     //   gpio_set_level(LED, 1);
//     // } else {
//     //   gpio_set_level(LED, 0);
//     // }
//   }
// }

// // Define the GPIO pin for the LED (GPIO 2 is common for onboard LEDs)
// #define BLINK_GPIO 2

// int app_main(void) {
//   // Configure the GPIO pin
//   gpio_reset_pin(LED);
//   gpio_set_direction(LED, GPIO_MODE_OUTPUT);

//   // Blink loop
//   while (1) {
//     // Turn LED ON
//     printf("LED ON\n");
//     gpio_set_level(LED, 1);
//     vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay 1 second

//     // Turn LED OFF
//     printf("LED OFF\n");
//     gpio_set_level(LED, 0);
//     vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay 1 second
//   }
// }