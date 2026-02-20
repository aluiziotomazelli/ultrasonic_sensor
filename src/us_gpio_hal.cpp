// LCOV_EXCL_START
// Since these are HAL components for the driver, it doesn't make sense to test them on host
#include "us_gpio_hal.hpp"
#include "driver/gpio.h"

esp_err_t GpioHAL::reset_pin(const gpio_num_t pin)
{
    return gpio_reset_pin(pin);
}

esp_err_t GpioHAL::config(const gpio_config_t &config)
{
    return gpio_config(&config);
}

esp_err_t GpioHAL::set_level(const gpio_num_t pin, const bool level)
{
    return gpio_set_level(pin, level ? 1 : 0);
}

esp_err_t GpioHAL::get_level(const gpio_num_t pin, bool &level)
{
    int val = gpio_get_level(pin);
    level = (val != 0);
    return ESP_OK;
}

esp_err_t GpioHAL::set_direction(const gpio_num_t pin, gpio_mode_t mode)
{
    return gpio_set_direction(pin, mode);
}

esp_err_t GpioHAL::set_drive_capability(gpio_num_t gpio_num, gpio_drive_cap_t strength)
{
    return gpio_set_drive_capability(gpio_num, strength);
}
// LCOV_EXCL_STOP
