idf_component_register(
    SRCS "calculation.c" "rssi_to_dist.c"
    INCLUDE_DIRS "."
    REQUIRES ble esp_driver_gpio
)