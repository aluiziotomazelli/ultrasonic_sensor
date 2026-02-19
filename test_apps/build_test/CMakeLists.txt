cmake_minimum_required(VERSION 3.16)

list(APPEND EXTRA_COMPONENT_DIRS "../../..")
list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/components")

set(COMPONENTS main ultrasonic_sensor)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(build_test)
