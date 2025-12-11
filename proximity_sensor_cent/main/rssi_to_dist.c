/**
 * Handles RSSI value to distance conversion and
 * displays those values to the monitor.
 */


#include "calculation.h"
#include "ble.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define CALIBRATED
// for converting rssi to distance with logreg
float m; // slope of log-linear regression
float b; // y intercept of log-linear regression
const int DIST_MEASURE_COUNT = 10;
const int DIST_TASK_DELAY_MS = 200;

const int NUM_CALIB_ROUNDS = 5; // 7 seg display only works with up to 9 rounds
const int RSSI_CALIB_MEASURE_COUNT = 30;
const int RSSI_CALIB_TASK_DELAY_MS = 250;

// Pin defs
#define LED         (gpio_num_t) 12
#define BUTTON_PIN  (gpio_num_t) 13
#define GPIO_STCP   (gpio_num_t) 27 // ST_CP (Storage Register Clock / Latch)
#define GPIO_SHCP   (gpio_num_t) 26 // SH_CP (Shift Register Clock)
#define GPIO_DS     (gpio_num_t) 25 // DS (Data Input)

// hex representations of the numbers 0 through 9
const uint8_t datArray[] = {
    0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f
};

static void shift_out_msb(gpio_num_t dataPin, gpio_num_t clockPin, uint8_t val) {
    for (int i = 0; i < 8; i++) {
        uint8_t bit = (val & (0x80 >> i)); 
        gpio_set_level(dataPin, bit ? 1 : 0);
        gpio_set_level(clockPin, 1);
        gpio_set_level(clockPin, 0);
    }
}

static void show_seven_segment(uint8_t digit_hex) {
    gpio_set_level(GPIO_STCP, 0); 
    shift_out_msb(GPIO_DS, GPIO_SHCP, digit_hex);
    gpio_set_level(GPIO_STCP, 1); 
    vTaskDelay(1); // yields to Idle task and resets watchdog
}

// GPIO setup
static void setup_gpio() {
    gpio_reset_pin(BUTTON_PIN);
    gpio_reset_pin(LED);    
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);

    gpio_set_direction(GPIO_STCP, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_SHCP, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_DS,   GPIO_MODE_OUTPUT);
    
    // Set initial levels
    gpio_set_level(GPIO_STCP, 0);
    gpio_set_level(GPIO_SHCP, 0);
    gpio_set_level(GPIO_DS, 0);
}

static int get_average_rssi(float *avg_rssi, const int measure_count, const int task_delay_ms) {
    float sum = 0.0f;
    int8_t rssi_val; 
    int valid_count = 0; 

    for (int i = 0; i < measure_count; i++) {
        rssi_val = get_rssi();

        // if not get_rssi error condition of -128
        if (rssi_val != -128) { // valid 
            ESP_LOGI("AVG_RSSI", "Current RSSI: %d", rssi_val);
            sum += (float)rssi_val; // adding ints to a float
            valid_count++;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
    
    if (valid_count == 0) {
        ESP_LOGW("AVG_RSSI", "No valid RSSI values");
        return -128; // error val
    } else {
        // divide only by the number of valid measurements
        *avg_rssi = sum / (float)valid_count; 
        return 0;
    }
}

static void distance_averaging_task(void) {
    float avg_rssi;
    float dist;
    int rc; // return code
    int dist_rc; // distance rc
    ESP_LOGI("CONVERSION", "RSSI to Distance beginning");
    while (1) {
        show_seven_segment(0x39); // 'C' indicator
        // task delay in get_average prevents this loop from running too fast
        rc = get_average_rssi(&avg_rssi, DIST_MEASURE_COUNT, DIST_TASK_DELAY_MS);
        if (rc != -128) {
            dist_rc = rssi_to_dist(&dist, avg_rssi, m, b);
            if (!dist_rc) {
                ESP_LOGI("CONVERSION", "RSSI Avg: %.2f  Distance (in): %.2f", avg_rssi, dist);
            } else {
                ESP_LOGW("CONVERSION", "Invalid Distance Conversion: M is 0");
            }
            
        }
    }
}

static void calibrate(void) {
    ESP_LOGI("CALIBRATION", "Calibrate started");
    float rssi_array[NUM_CALIB_ROUNDS];
    float avg_rssi;
    int rc;
    int i = 0;

    while (i < NUM_CALIB_ROUNDS) {
            gpio_set_level(LED, 1); // light on indicate move cali location
            if (NUM_CALIB_ROUNDS <= 10) {
                show_seven_segment(datArray[i]); // shows round number indexed from 0
            } else {
                // if rounds are > 10 light up entire 7 seg display
                show_seven_segment(0xFF);
            }
            // on btn click
        if (gpio_get_level(BUTTON_PIN)) {
            ESP_LOGI("CALIBRATION", "Next distance measurement cycle triggered");
            gpio_set_level(LED, 0); // light off indicate calibration started
            // Wait for button release
            while (gpio_get_level(BUTTON_PIN)) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            rc = get_average_rssi(&avg_rssi,RSSI_CALIB_MEASURE_COUNT, RSSI_CALIB_TASK_DELAY_MS);
            if (rc != -128) {
                rssi_array[i] = avg_rssi;
                ESP_LOGI("CALIBRATION", "Calibration mean[%d] = %.2f", i, rssi_array[i]);
                i++;
            } else {
                ESP_LOGW("CALIBRATION", "No valid rssi values found, restarting round");
            }
      }
    }
    rssi_logreg_to_params(NUM_CALIB_ROUNDS, rssi_array, &m, &b);
}

void distance_conversion_task(void *pvParameters) {
    setup_gpio();
    #ifndef CALIBRATED // if it's not calibrated calibrate
        calibrate();
    #else
        m = -12;
        b = -35;
    #endif
    distance_averaging_task();
}

