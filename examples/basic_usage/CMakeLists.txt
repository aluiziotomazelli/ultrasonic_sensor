cmake_minimum_required(VERSION 3.16)

list(APPEND EXTRA_COMPONENT_DIRS 
    "../../" 
    "../../external"    # idf_hals
    
)

set(COMPONENTS main ultrasonic_sensor idf_hals)


include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(basic_usage_example)
