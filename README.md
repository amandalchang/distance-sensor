# Distance Sensor Project

In this project, we've recreated receiver & transmitter devices (similar to an Apple AirTag) that communicate over bluetooth low energy (BLE) using 2 ESP32 boards, a 7 segment display, a button, and LEDs.

### Features

- Realtime monitoring of distance
- RSSI to distance in meters calibration between two ESP32 boards
  - GPIO button input sensing
  - LED output feedback
  - 7 segment display output for feedback during calibration

### Architecture

We built our project off of the [ESP-IDF proximity sensor example code](https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/nimble/ble_proximity_sensor). This example code has 2 projects with in it, one for transmitter and one for receiver board. Our distance calculating additions only modifies the _proximit_sensor_cent_ project, leaving the _proximity_sensor_prph_ projectun-touched (only for flashing purposes).

- BLE stack managed by NimBLE
- FreeRTOS multithreading for tasks
- Button, 7 segment display, LEDs connected via GPIO

## How to run

### Hardware Requirements

All our hardware is wired on a breadboard.

- A development board with ESP32/ESP32-C2/ESP32-C3/ESP32-S3 SoC (e.g., ESP32-DevKitC, ESP-WROVER-KIT, etc.)
- A USB cable for power supply and programming
- 7 segment LED display and driver chip
- 2 single LEDs
- Button/switch

### Dependencies

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html#get-started-get-esp-idf)

### Clone Repo

In terminal:

```
git@github.com:amandalchang/distance-sensor.git
cd distance-sensor
```

### Compile & Run with ESP-IDF

Navigate to /distance-sensor/proximity_sensor_cent, and build and flash your **receiver board**:

```
make compile
make flash
```

To flash your **transmitter board**, navigate to /distance-sensor/proximity_sensor_prph. Run the same lines as the receiver board above.

### Calibration

By default, pre-calibrated params are entered in the file _rssi_to_dist.c_ lines 162-163 so the calibration stage is skipped and starts displaying distance immediately. However, if you want to enable the calibration stage, undefine the variable `CALIBRATED` in line 13.
