#pragma once

#include "esp_err.h"
#include "driver/gpio.h"

/**
 * @interface IGpioHAL
 * @brief Hardware Abstraction Layer for GPIO operations
 * @internal
 */
class IGpioHAL
{
public:
    virtual ~IGpioHAL() = default;

    /** @internal */
    virtual esp_err_t reset_pin(const gpio_num_t pin) = 0;

    /** @internal */
    virtual esp_err_t config(const gpio_config_t &config) = 0;

    /** @internal */
    virtual esp_err_t set_level(const gpio_num_t pin, bool level) = 0;

    /** @internal */
    virtual esp_err_t get_level(const gpio_num_t pin, bool &level) = 0;

    /** @internal */
    virtual esp_err_t set_direction(const gpio_num_t pin, gpio_mode_t mode) = 0;

    /** @internal */
    virtual esp_err_t set_drive_capability(const gpio_num_t gpio_num, gpio_drive_cap_t strength) = 0;
};