#pragma once

#include "esp_err.h"
#include "i_us_driver.hpp"
#include "us_types.hpp"

/**
 * @interface IUltrasonicSensor
 * @brief Public interface for the ultrasonic sensor orchestrator.
 *
 * Orchestrates multiple pings via IUsDriver, applies statistical processing
 * via IUsProcessor, and returns a unified result.
 */
class IUsSensor
{
public:
    virtual ~IUsSensor() = default;

    /**
     * @brief Initializes the sensor hardware.
     * Must be called before read_distance(). Allows the chip to stabilize
     * before touching GPIO pins.
     */
    virtual esp_err_t init() = 0;

    /**
     * @brief Deinitializes the sensor hardware and resets pins to a safe state.
     */
    virtual esp_err_t deinit() = 0;

    /**
     * @brief Performs a distance measurement using multiple pings.
     *
     * Hardware failures (ECHO_STUCK, HW_FAULT) abort the ping loop immediately.
     * Logical failures (TIMEOUT, OUT_OF_RANGE) discard the ping and continue.
     *
     * @param ping_count Number of pings to attempt. Caller can vary this at
     *                   runtime based on quality or error conditions.
     * @return Reading with unified UsResult and distance in cm.
     *
     * Usage:
     *   auto r = sensor.read_distance(7);
     *   if (is_success(r.result)) { use(r.cm); }
     *   else if (r.result == UsResult::ECHO_STUCK) { power_cycle(); }
     */
    virtual Reading read_distance(uint8_t ping_count) = 0;
};
