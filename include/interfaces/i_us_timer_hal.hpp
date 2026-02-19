#pragma once

#include "esp_err.h"
#include <cstdint>

/**
 * @interface ITimerHAL
 * @brief Hardware Abstraction Layer for Timing and Delays
 */
class ITimerHAL
{
public:
    virtual ~ITimerHAL() = default;

    /**
     * @brief Gets current timestamp in microseconds.
     */
    virtual uint64_t get_now_us() = 0;

    /**
     * @brief Precise delay in microseconds.
     */
    virtual esp_err_t delay_us(uint32_t us) = 0;

    /**
     * @brief OS-friendly delay in milliseconds.
     */
    virtual esp_err_t delay_ms(uint32_t ms) = 0;
};
