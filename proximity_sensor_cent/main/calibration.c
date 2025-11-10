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
#define BUTTON 2
#define LED 5

// https://randomnerdtutorials.com/esp-idf-esp32-gpio-inputs/
#define BUTTON_PIN 4

gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << BUTTON_PIN),   // Select GPIO 4
    .mode = GPIO_MODE_INPUT,                  // Set as input
    .pull_up_en = GPIO_PULLUP_ENABLE,     // Enable internal pull-up
    .pull_down_en = GPIO_PULLDOWN_DISABLE, // Disable pull-down
    .intr_type = GPIO_INTR_DISABLE        // Disable interrupts
};
gpio_config(&io_conf);

// create gpio button
button_config_t gpio_btn_cfg = {
    .type = BUTTON_TYPE_GPIO,
    .long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
    .short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
    .gpio_button_config =
        {
            .gpio_num = 0,
            .active_level = 0,
        },
};
button_handle_t calibration_btn = iot_button_create(&gpio_btn_cfg);
if (NULL == gpio_btn) {
  ESP_LOGE(TAG, "Button create failed");
}

int main(void) {
    gpio_get_level(BUTTON_PIN);

  while (1) {
    int curr_button_state = gpio_get_level(calibration_btn);
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