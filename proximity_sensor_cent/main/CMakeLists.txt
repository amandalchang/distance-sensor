set(srcs "new_main.c" "calculation.c" "ble.c" "rssi_to_dist.c")

# rssi_to_dist.c
idf_component_register(SRCS "new_main.c" "${srcs}"
                       INCLUDE_DIRS ".")
