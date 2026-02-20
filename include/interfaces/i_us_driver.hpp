#pragma once

#include "esp_err.h"
#include "us_types.hpp"
#include "driver/gpio.h"

/**
 * @brief Interface for the low-level ultrasonic hardware driver.
 * @internal
 *
 * Handles the raw protocol for a single ultrasonic measurement.
 */
class IUsDriver
{
public:
    virtual ~IUsDriver() = default;

    /** @internal */
    virtual esp_err_t init(uint16_t warmup_time_ms = 0) = 0;

    /** @internal */
    virtual esp_err_t deinit() = 0;

    /** @internal */
    virtual Reading ping_once(const UsConfig &cfg) = 0;
};
