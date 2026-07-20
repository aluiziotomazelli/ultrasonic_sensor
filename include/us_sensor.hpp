#pragma once

#include <memory>

#include "esp_err.h"
#include "i_us_driver.hpp"
#include "i_us_processor.hpp"
#include "i_us_sensor.hpp"
#include "us_types.hpp"
#include "interfaces/i_hal_gpio.hpp"
#include "interfaces/i_hal_timer.hpp"
#include "interfaces/i_hal_sys_rom.hpp"
#include "interfaces/i_hal_freertos.hpp"

namespace ultrasonic {

/**
 * @brief Concrete implementation of IUsSensor.
 *
 * This class orchestrates multiple pings via IUsDriver and applies statistical
 * processing via IUsProcessor to produce a final Reading.
 *
 * It manages the high-level measurement cycle, handling hardware failures
 * immediately and logical failures (like timeouts or out-of-range pings) by
 * collecting them for statistical analysis.
 */
class UsSensor : public IUsSensor
{
public:
    /**
     * @brief Construct a new UsSensor object.
     *
     * This constructor is intended for production use and automatically creates
     * the necessary internal components (driver, processor).
     *
     * @param gpio_hal     GPIO HAL reference.
     * @param timer_hal    Timer HAL reference.
     * @param sys_rom_hal  System ROM HAL reference.
     * @param freertos_hal FreeRTOS HAL reference.
     * @param trig_pin     GPIO number for the trigger pin.
     * @param echo_pin     GPIO number for the echo pin.
     * @param cfg          Configuration structure for the sensor.
     */
    UsSensor(
        idf_hals::IGpioHAL &gpio_hal,
        idf_hals::ITimerHAL &timer_hal,
        idf_hals::ISysRomHAL &sys_rom_hal,
        idf_hals::IHalFreertos &freertos_hal,
        gpio_num_t trig_pin,
        gpio_num_t echo_pin,
        const UsConfig &cfg);

    /**
     * @brief Construct a new UsSensor object with dependency injection.
     *
     * This constructor is primarily intended for unit testing or advanced
     * customization.
     *
     * @param cfg          Configuration structure for the sensor.
     * @param driver       Shared pointer to a driver implementation.
     * @param processor    Shared pointer to a processor implementation.
     * @param freertos_hal FreeRTOS HAL reference.
     */
    UsSensor(
        const UsConfig &cfg,
        std::shared_ptr<IUsDriver> driver,
        std::shared_ptr<IUsProcessor> processor,
        idf_hals::IHalFreertos &freertos_hal);

    ~UsSensor() override = default;

    /** @copydoc IUsSensor::init() */
    esp_err_t init() override;

    /** @copydoc IUsSensor::deinit() */
    esp_err_t deinit() override;

    /** @copydoc IUsSensor::read_distance() */
    Reading read_distance(uint8_t ping_count) override;

private:
    /** @internal */
    UsConfig cfg_;
    /** @internal */
    std::shared_ptr<IUsDriver> driver_;
    /** @internal */
    std::shared_ptr<IUsProcessor> processor_;
    /** @internal */
    idf_hals::IHalFreertos &freertos_hal_;

    /** @internal */
    static constexpr uint8_t MAX_PINGS = 15;
};

} // namespace ultrasonic
