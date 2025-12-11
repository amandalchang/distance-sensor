idf_component_register(
    SRCS "ble.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_driver_gpio bt nimble_central_utils
)