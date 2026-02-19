#pragma once

#include <memory>

#include "i_us_driver.hpp"
#include "i_us_processor.hpp"
#include "i_us_sensor.hpp"
#include "us_types.hpp"

/**
 * @class UltrasonicSensor
 * @brief Concrete implementation of IUltrasonicSensor.
 *
 * Orchestrates multiple pings via IUsDriver and applies statistical
 * processing via IUsProcessor to produce a final Reading.
 *
 * Hardware failures (ECHO_STUCK, HW_FAULT) abort the ping loop immediately.
 * Logical failures (TIMEOUT, OUT_OF_RANGE) discard the ping and continue.
 */
class UsSensor : public IUsSensor
{
public:
    /**
     * @brief Constructor for application use. Creates internal concrete HALs and driver.
     */
    UsSensor(gpio_num_t trig_pin, gpio_num_t echo_pin, const UsConfig &cfg);

    /**
     * @brief Constructor for testing (Dependency Injection).
     */
    UsSensor(const UsConfig &cfg, std::shared_ptr<IUsDriver> driver, std::shared_ptr<IUsProcessor> processor);

    ~UsSensor() override = default;

    /** @copydoc IUltrasonicSensor::init() */
    esp_err_t init() override;

    /** @copydoc IUltrasonicSensor::deinit() */
    esp_err_t deinit() override;

    /** @copydoc IUltrasonicSensor::read_distance() */
    Reading read_distance(uint8_t ping_count) override;

private:
    UsConfig cfg_;
    std::shared_ptr<IUsDriver> driver_;
    std::shared_ptr<IUsProcessor> processor_;

    static constexpr uint8_t MAX_PINGS = 15;
};
