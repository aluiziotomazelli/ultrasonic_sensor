idf_component_register(
    SRCS
        "src/us_gpio_hal.cpp"
        "src/us_timer_hal.cpp"
        "src/us_driver.cpp"
        "src/us_processor.cpp"
        "src/us_sensor.cpp"
    INCLUDE_DIRS
        "include"
        "include/interfaces"
    REQUIRES
        driver
        esp_timer
)

if(IDF_TARGET STREQUAL "linux")
    target_compile_options(${COMPONENT_LIB} PUBLIC --coverage)
    target_link_options(${COMPONENT_LIB} PUBLIC --coverage)
endif()