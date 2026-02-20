// LCOV_EXCL_START
// Since these are HAL components for the driver, it doesn't make sense to test them on host
#pragma once

#include "i_us_timer_hal.hpp"

/**
 * @brief Concrete implementation of ITimerHAL using ESP-IDF functions.
 * @internal
 */
class TimerHAL : public ITimerHAL
{
public:
    /** @internal */
    TimerHAL() = default;
    ~TimerHAL() override = default;

    /** @copydoc ITimerHAL::get_now_us() */
    uint64_t get_now_us() override;

    /** @copydoc ITimerHAL::delay_us() */
    esp_err_t delay_us(uint32_t us) override;

    /** @copydoc ITimerHAL::delay_ms() */
    esp_err_t delay_ms(uint32_t ms) override;
};
// LCOV_EXCL_STOP
