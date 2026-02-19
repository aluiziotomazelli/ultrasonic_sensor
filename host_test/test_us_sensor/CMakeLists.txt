cmake_minimum_required(VERSION 3.16)

# Append extra component directories so IDF can find our local components.
list(APPEND EXTRA_COMPONENT_DIRS 
    "../.."                              # The 'ultrasonic_sensor' component being tested
    "../gtest"                           # The GTest wrapper component
    "$ENV{IDF_PATH}/tools/mocks/driver"  # Official ESP-IDF driver mocks
    "$ENV{IDF_PATH}/tools/mocks/esp_timer" # esp_timer mocks
)

# Explicitly list the components to be included in the build.
set(COMPONENTS main ultrasonic_sensor)

# Standard ESP-IDF project configuration.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test_us_sensor)

# Include common coverage logic
include("../coverage_common.cmake")
setup_gtest_coverage(test_us_sensor test_us_sensor.elf)
