#pragma once

#include "i_us_timer_hal.hpp"

/**
 * @class TimerHAL
 * @brief Concrete implementation of ITimerHAL using ESP-IDF functions.
 */
class TimerHAL : public ITimerHAL
{
public:
    TimerHAL() = default;
    ~TimerHAL() override = default;

    /** @copydoc ITimerHAL::get_now_us() */
    uint64_t get_now_us() override;

    /** @copydoc ITimerHAL::delay_us() */
    esp_err_t delay_us(uint32_t us) override;

    /** @copydoc ITimerHAL::delay_ms() */
    esp_err_t delay_ms(uint32_t ms) override;
};
