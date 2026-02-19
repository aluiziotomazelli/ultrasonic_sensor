idf_component_register(
    SRCS 
        "main.cpp"
        "test_us_sensor.cpp"
    INCLUDE_DIRS 
        "."
    REQUIRES 
        gtest
        ultrasonic_sensor
        
    WHOLE_ARCHIVE
)

if(IDF_TARGET STREQUAL "linux")
    target_compile_options(${COMPONENT_LIB} PUBLIC --coverage)
    target_link_options(${COMPONENT_LIB} PUBLIC --coverage)
endif()