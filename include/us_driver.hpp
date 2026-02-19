#pragma once

#include <memory>

#include "esp_err.h"
#include "i_us_driver.hpp"
#include "i_us_gpio_hal.hpp"
#include "i_us_timer_hal.hpp"

/**
 * @class UsDriver
 * @brief Concrete implementation of IUsDriver for HC-SR04-compatible sensors.
 *
 * Handles the low-level GPIO protocol: trigger pulse, echo detection, and
 * pulse duration measurement. Maps all hardware errors to UsResult.
 */
class UsDriver : public IUsDriver
{
public:
    static constexpr float SOUND_SPEED_CM_PER_US = 0.0343f;

    UsDriver(
        std::shared_ptr<IGpioHAL> gpio_hal,
        std::shared_ptr<ITimerHAL> timer_hal,
        gpio_num_t trig_pin,
        gpio_num_t echo_pin);

    ~UsDriver() override = default;

    /** @copydoc IUsDriver::init() */
    esp_err_t init(uint16_t warmup_time_ms = 0) override;

    /** @copydoc IUsDriver::deinit() */
    esp_err_t deinit() override;

    /** @copydoc IUsDriver::ping_once() */
    Reading ping_once(const UsConfig &cfg) override;

private:
    /**
     * @brief Checks if the ECHO pin is stuck HIGH before triggering.
     * @return true if stuck (ECHO_STUCK condition).
     */
    bool is_echo_stuck();

    /**
     * @brief Sends the trigger pulse.
     * @return ESP_OK or error from HAL.
     */
    esp_err_t trigger(uint16_t pulse_duration_us);

    /**
     * @brief Waits for the ECHO pin to go HIGH (rising edge).
     * @return ESP_OK, ESP_ERR_TIMEOUT, or HAL error.
     */
    esp_err_t wait_rising_edge(uint32_t timeout_us);

    /**
     * @brief Measures the duration of the HIGH pulse on ECHO.
     * @param[out] duration_us Measured pulse duration.
     * @return ESP_OK, ESP_ERR_TIMEOUT, or HAL error.
     */
    esp_err_t measure_pulse(uint32_t timeout_us, uint32_t &duration_us);

    std::shared_ptr<IGpioHAL> gpio_hal_;
    std::shared_ptr<ITimerHAL> timer_hal_;

    gpio_num_t trig_pin_;
    gpio_num_t echo_pin_;
};