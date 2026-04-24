// LCOV_EXCL_START
// Since these are HAL components for the driver, it doesn't make sense to test them on host
#pragma once

#include "driver/gpio.h"
#include "i_us_gpio_hal.hpp"

namespace ultrasonic {

/**
 * @brief Concrete implementation of IGpioHAL using ESP-IDF driver.
 * @internal
 */
class GpioHAL : public IGpioHAL
{
public:
    /** @internal */
    GpioHAL() = default;
    ~GpioHAL() override = default;

    /** @copydoc IGpioHAL::reset_pin() */
    esp_err_t reset_pin(const gpio_num_t pin) override
    {
        return gpio_reset_pin(pin);
    }

    /** @copydoc IGpioHAL::config() */
    esp_err_t config(const gpio_config_t &config) override
    {
        return gpio_config(&config);
    }

    /** @copydoc IGpioHAL::set_level() */
    esp_err_t set_level(const gpio_num_t pin, const bool level) override
    {
        return gpio_set_level(pin, level ? 1 : 0);
    }

    /** @copydoc IGpioHAL::get_level() */
    esp_err_t get_level(const gpio_num_t pin, bool &level) override
    {
        int val = gpio_get_level(pin);
        level = (val != 0);
        return ESP_OK;
    }

    /** @copydoc IGpioHAL::set_direction() */
    esp_err_t set_direction(const gpio_num_t pin, gpio_mode_t mode) override
    {
        return gpio_set_direction(pin, mode);
    }

    /** @copydoc IGpioHAL::set_drive_capability() */
    esp_err_t set_drive_capability(const gpio_num_t gpio_num, gpio_drive_cap_t strength) override
    {
        return gpio_set_drive_capability(gpio_num, strength);
    }
};

} // namespace ultrasonic
// LCOV_EXCL_STOP
