/*
Calibration file for RSSI to meters
*/

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iot_button.h"
#include "sdkconfig.h"
#include <stdio.h>

// define pins
#define BUTTON_PIN 4
#define LED 5

// https://randomnerdtutorials.com/esp-idf-esp32-gpio-inputs/

// gpio_config_t io_conf = {
//     .pin_bit_mask = (1ULL << BUTTON_PIN),   // Select GPIO 4
//     .mode = GPIO_MODE_INPUT,                  // Set as input
//     .pull_up_en = GPIO_PULLUP_ENABLE,     // Enable internal pull-up
//     .pull_down_en = GPIO_PULLDOWN_DISABLE, // Disable pull-down
//     .intr_type = GPIO_INTR_DISABLE        // Disable interrupts
// };
// gpio_config(&io_conf);

int main(void) {
    // configure
    gpio_reset_pin(BUTTON_PIN);
    gpio_reset_pin(LED);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);

  while (1) {
    // int curr_button_state = gpio_get_level(calibration_btn);
    if (gpio_get_level(BUTTON_PIN)) {
        gpio_set_level(LED, 1);
    } else {
        gpio_set_level(LED, 0);
    }
  }
}

// // Define the GPIO pin for the LED (GPIO 2 is common for onboard LEDs)
// #define BLINK_GPIO 2

// void app_main(void) {
//   // Configure the GPIO pin
//   gpio_reset_pin(BLINK_GPIO);
//   gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

//   // Blink loop
//   while (1) {
//     // Turn LED ON
//     printf("LED ON\n");
//     gpio_set_level(BLINK_GPIO, 1);
//     vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay 1 second

//     // Turn LED OFF
//     printf("LED OFF\n");
//     gpio_set_level(BLINK_GPIO, 0);
//     vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay 1 second
//   }
// }