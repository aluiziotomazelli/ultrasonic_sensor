#pragma once

#include "esp_err.h"
#include "i_us_driver.hpp"
#include "interfaces/i_hal_gpio.hpp"
#include "interfaces/i_hal_timer.hpp"
#include "interfaces/i_hal_sys_rom.hpp"

namespace ultrasonic {

/**
 * @brief Concrete implementation of IUsDriver for HC-SR04-compatible sensors.
 * @internal
 *
 * Handles the low-level GPIO protocol: trigger pulse, echo detection, and
 * pulse duration measurement. Maps hardware errors to UsResult.
 */
class UsDriver : public IUsDriver
{
public:
    /** @internal */
    static constexpr float SOUND_SPEED_CM_PER_US = 0.0343f;

    /** @internal */
    UsDriver(
        idf_hals::IGpioHAL &gpio_hal,
        idf_hals::ITimerHAL &timer_hal,
        idf_hals::ISysRomHAL &sys_rom_hal,
        gpio_num_t trig_pin,
        gpio_num_t echo_pin);

    ~UsDriver() override = default;

    /** @copydoc IUsDriver::init() */
    esp_err_t init() override;

    /** @copydoc IUsDriver::deinit() */
    esp_err_t deinit() override;

    /** @copydoc IUsDriver::ping_once() */
    Reading ping_once(const UsConfig &cfg) override;

private:
    /** @internal */
    bool is_echo_stuck();

    /** @internal */
    esp_err_t trigger(uint16_t pulse_duration_us);

    /** @internal */
    esp_err_t wait_rising_edge(uint32_t timeout_us);

    /** @internal */
    esp_err_t measure_pulse(uint32_t timeout_us, uint32_t &duration_us);

    /** @internal */
    idf_hals::IGpioHAL &gpio_hal_;
    /** @internal */
    idf_hals::ITimerHAL &timer_hal_;
    /** @internal */
    idf_hals::ISysRomHAL &sys_rom_hal_;

    /** @internal */
    gpio_num_t trig_pin_;
    /** @internal */
    gpio_num_t echo_pin_;
};

} // namespace ultrasonic
