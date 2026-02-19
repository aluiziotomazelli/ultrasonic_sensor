#pragma once

#include "esp_err.h"
#include "us_types.hpp"
#include "driver/gpio.h"

/**
 * @interface IUsDriver
 * @brief Interface for the low-level ultrasonic hardware driver.
 *
 * Responsible for a single ping: trigger the sensor and measure the echo pulse.
 * Does NOT orchestrate multiple pings or apply filters — that is IUltrasonicSensor's job.
 */
class IUsDriver
{
public:
    virtual ~IUsDriver() = default;

    /**
     * @brief Initializes GPIO pins and waits for the sensor to stabilize.
     *
     * Must be called before ping_once(). Separate from constructor to allow
     * the chip to stabilize before touching pins.
     *
     * @param warmup_time_ms Time to wait after GPIO setup (from UltrasonicConfig).
     *                       Pass 0 to skip warmup (e.g., in tests).
     */
    virtual esp_err_t init(uint16_t warmup_time_ms = 0) = 0;

    /**
     * @brief Deinitializes and resets GPIO pins to a safe state.
     * @note Reserved for future use (e.g., power-cycling after ECHO_STUCK).
     */
    virtual esp_err_t deinit() = 0;

    /**
     * @brief Performs a single ultrasonic ping and returns the result.
     *
     * Error mapping:
     *   - ECHO pin HIGH before trigger → UsResult::ECHO_STUCK
     *   - HAL operation failure        → UsResult::HW_FAULT
     *   - No echo within timeout_us    → UsResult::TIMEOUT
     *   - Distance outside cfg limits  → UsResult::OUT_OF_RANGE
     *   - Valid distance               → UsResult::OK
     *
     * @param cfg Sensor configuration (timeout, pulse duration, distance limits).
     * @return Reading with result and distance (cm valid only if is_success).
     */
    virtual Reading ping_once(const UsConfig &cfg) = 0;
};