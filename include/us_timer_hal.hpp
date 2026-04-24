// LCOV_EXCL_START
// Since these are HAL components for the driver, it doesn't make sense to test them on host
#pragma once

#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i_us_timer_hal.hpp"

namespace ultrasonic {

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
    uint64_t get_now_us() override
    {
        return (uint64_t)esp_timer_get_time();
    }

    /** @copydoc ITimerHAL::delay_us() */
    esp_err_t delay_us(uint32_t us) override
    {
        esp_rom_delay_us(us);
        return ESP_OK;
    }

    /** @copydoc ITimerHAL::delay_ms() */
    esp_err_t delay_ms(uint32_t ms) override
    {
        vTaskDelay(pdMS_TO_TICKS(ms));
        return ESP_OK;
    }
};

} // namespace ultrasonic
// LCOV_EXCL_STOP
