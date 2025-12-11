# Distance Sensor Project

In this project, we've recreated tracker & finder devices (similar to an Apple AirTag) that communicate over bluetooth low energy (BLE) using 2 ESP32 boards, a 7 segment display, a button, and LEDs.

### Features

- Realtime monitoring of distance
- RSSI to distance in meters calibration between two ESP32 boards
  - GPIO button reading
  - 7 segment display output for feedback during calibration

### Architecture

- BLE stack managed by NimBLE
- FreeRTOS multithreading for tasks
- Button, 7 segment display, LEDs connected via GPIO

## How to run

#### Run with ESP-IDF

Set up ESP environment and variables via terminal:

```
cd ~/esp/esp-idf
source export.sh
```

Navigate to /distance-sensor/proximity_sensor_cent, and build and flash your **receiver board**:

```
idf.py set-target esp32
idf.py build
idf.py flash monitor
```

To flash your **transmitter board**, navigate to /distance-sensor/proximity_sensor_prph. Run the same lines as the receiver board above.
