# Use the latest C standard supported by Clang and LLVM in the latest LTS
# version of Ubuntu.
set(CMAKE_C_STANDARD 23)

# set(CMAKE_C_FLAGS "-Wall -Wextra") # This breaks everything!!
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wconversion")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wdouble-promotion")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wduplicated-branches")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wduplicated-cond")
# The following line is only needed in LLVM/clang
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wimplicit-fallthrough")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wlogical-op")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmisleading-indentation")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wnull-dereference")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wshadow")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wsign-conversion")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunreachable-code")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunused")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")

# # No options are set here because the configuration is set in .clang-tidy
# # instead. If that file is not present, clang-tidy will be of limited help.
# set(CMAKE_C_CLANG_TIDY "clang-tidy")

idf_component_register(
    SRCS "calculation.c" "rssi_to_dist.c"
    INCLUDE_DIRS "."
    REQUIRES ble esp_driver_gpio
)