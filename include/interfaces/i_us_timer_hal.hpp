#pragma once

#include "esp_err.h"
#include <cstdint>

/**
 * @brief Hardware Abstraction Layer for Timing and Delays.
 * @internal
 */
class ITimerHAL
{
public:
    virtual ~ITimerHAL() = default;

    /** @internal */
    virtual uint64_t get_now_us() = 0;

    /** @internal */
    virtual esp_err_t delay_us(uint32_t us) = 0;

    /** @internal */
    virtual esp_err_t delay_ms(uint32_t ms) = 0;
};
