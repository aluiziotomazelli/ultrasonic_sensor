#pragma once

#include "esp_err.h"
#include "us_types.hpp"

/**
 * @brief Public interface for the ultrasonic sensor orchestrator.
 *
 * This class defines the high-level API for interacting with the ultrasonic sensor.
 * It is responsible for orchestrating multiple pings, applying statistical filters
 * to the samples, and returning a reliable distance measurement.
 */
class IUsSensor
{
public:
    virtual ~IUsSensor() = default;

    /**
     * @brief Initialize the ultrasonic sensor component.
     *
     * This method configures the necessary GPIOs and prepares the sensor for measurements.
     * It may also include a warmup period to allow the sensor hardware to stabilize.
     *
     * @return
     *     - ESP_OK: Success
     *     - Other: error codes propagated from the underlying driver HAL implementation
     */
    virtual esp_err_t init() = 0;

    /**
     * @brief Deinitialize the ultrasonic sensor component.
     *
     * This method releases any resources used by the sensor and resets GPIO pins
     * to a safe state.
     *
     * @return
     *     - ESP_OK: Success
     *     - Other: error codes propagated from the underlying driver HAL implementation
     */
    virtual esp_err_t deinit() = 0;

    /**
     * @brief Perform a distance measurement using multiple pings.
     *
     * This method triggers the sensor multiple times, collects the results, and
     * processes them using the configured statistical filter.
     *
     * @param ping_count Number of pings to attempt (limited by IUsProcessor::MAX_PINGS).
     *
     * @return Reading structure containing the unified UsResult and the filtered
     *         distance in centimeters.
     *
     * @note Logical failures (TIMEOUT, OUT_OF_RANGE) for individual pings are handled
     *       internally. If the final result is INSUFFICIENT_SAMPLES, it may be
     *       refined to the most frequent logical error encountered.
     */
    virtual Reading read_distance(uint8_t ping_count) = 0;
};
