cmake_minimum_required(VERSION 3.16)
project(ultrasonic_sensor_host_tests NONE)

# Enable CTest for this project
include(CTest)

# Define the tests for CTest.
# These commands assume that each project has been built using 'idf.py build'
# and that CTest is being run from a 'build' subdirectory within 'host_test'.
add_test(NAME test_us_driver
         COMMAND ../test_us_driver/build/test_us_driver.elf)
add_test(NAME test_us_processor
         COMMAND ../test_us_processor/build/test_ultrasonic_sensor.elf)
add_test(NAME test_us_sensor
         COMMAND ../test_us_sensor/build/test_us_sensor.elf)

# Unified Coverage Configuration
find_program(LCOV_PATH lcov REQUIRED)
find_program(GENHTML_PATH genhtml REQUIRED)

# The unified coverage report will be placed in host_test/coverage
set(UNIFIED_COVERAGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/coverage")

# This target aggregates coverage data (.gcda files) from all test build directories
add_custom_target(unified_coverage
    COMMAND ${LCOV_PATH} --capture --directory .. --output-file unified_coverage.info --rc branch_coverage=1 --rc geninfo_unexecuted_blocks=1 --ignore-errors mismatch,inconsistent
    COMMAND ${LCOV_PATH} --extract unified_coverage.info "*/ultrasonic_sensor/src/*" "*/ultrasonic_sensor/include/*" --output-file unified_coverage_filtered.info --rc branch_coverage=1 --ignore-errors mismatch,inconsistent,unused,empty
    COMMAND ${GENHTML_PATH} unified_coverage_filtered.info --output-directory ${UNIFIED_COVERAGE_DIR} --title "Ultrasonic Sensor Unified Coverage" --rc branch_coverage=1 --ignore-errors mismatch,inconsistent,unused
    COMMAND ${LCOV_PATH} --list unified_coverage_filtered.info --rc branch_coverage=1 --ignore-errors mismatch,inconsistent,unused,empty
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating unified coverage report from all host tests"
)

# Helper target to build all tests at once (requires idf.py to be in your PATH)
add_custom_target(build_all_tests
    COMMAND idf.py -C ../test_us_driver build
    COMMAND idf.py -C ../test_us_processor build
    COMMAND idf.py -C ../test_us_sensor build
    COMMENT "Building all test projects using idf.py"
)
