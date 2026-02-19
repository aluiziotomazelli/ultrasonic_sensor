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
project(test_us_driver)

if(IDF_TARGET STREQUAL "linux")
    find_program(LCOV_PATH lcov REQUIRED)
    find_program(GENHTML_PATH genhtml REQUIRED)

    set(COVERAGE_DIR "${PROJECT_SOURCE_DIR}/coverage")

    # Custom target to run tests and generate a filtered coverage report
    add_custom_target(generate_coverage
        COMMAND find . -name "*.gcda" -delete
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${COVERAGE_DIR}
        COMMAND ./test_us_driver.elf
        COMMAND ${LCOV_PATH} --capture --directory . --output-file coverage.info --rc branch_coverage=1 --ignore-errors mismatch,inconsistent
        COMMAND ${LCOV_PATH} --extract coverage.info "*/ultrasonic_sensor/src/*" "*/ultrasonic_sensor/include/*" --output-file coverage_filtered.info --rc branch_coverage=1 --ignore-errors mismatch,inconsistent,unused,empty
        COMMAND ${GENHTML_PATH} coverage_filtered.info --output-directory ${COVERAGE_DIR} --title "Ultrasonic Sensor Component Coverage" --rc branch_coverage=1 --ignore-errors mismatch,inconsistent,unused
        COMMAND ${LCOV_PATH} --list coverage_filtered.info --rc branch_coverage=1 --ignore-errors mismatch,inconsistent,unused,empty
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running tests and generating filtered coverage report"
    )
endif()
