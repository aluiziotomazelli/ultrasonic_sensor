#pragma once

#include <cmath>
#include <cstdint>

/**
 * @brief Unified result type for all ultrasonic sensor operations.
 *
 * Use is_success() to check if a reading is valid before using the distance.
 *
 * Usage:
 *   auto reading = sensor.read_distance(7);
 *   if (is_success(reading.result)) {
 *       use_distance(reading.cm);
 *   } else if (reading.result == UsResult::ECHO_STUCK) {
 *       power_cycle_sensor();
 *   }
 */
enum class UsResult
{
    // Success — distance value is valid
    OK,          /**< Reliable reading, high ping ratio, low variance. */
    WEAK_SIGNAL, /**< Valid reading but ping ratio is below the ideal threshold. */

    // Logical failures — not enough valid data
    TIMEOUT,              /**< Sensor did not respond to trigger within timeout_us. */
    OUT_OF_RANGE,         /**< Measured distance is outside [min_distance_cm, max_distance_cm]. Also
                             indicates a possible configuration error. */
    HIGH_VARIANCE,        /**< Standard deviation of valid pings exceeds max_dev_cm. */
    INSUFFICIENT_SAMPLES, /**< Too few valid pings (ratio below minimum threshold). */

    // Hardware failures — require application-level action
    ECHO_STUCK, /**< ECHO pin is stuck HIGH. Application should power-cycle the sensor. */
    HW_FAULT,   /**< GPIO/HAL operation failed (set_level, get_level returned an error). */
};

/**
 * @brief Result of a single ping attempt.
 */
struct Reading
{
    UsResult result;
    float cm; /**< Distance in centimeters. Only valid if is_success(result). Otherwise is 0 */

    bool operator==(const Reading &other) const
    {
        return (result == other.result) &&
               ((result != UsResult::OK && result != UsResult::WEAK_SIGNAL) || std::abs(cm - other.cm) < 0.001f);
    }

    bool operator!=(const Reading &other) const { return !(*this == other); }
};

/**
 * @brief Returns true if the result represents a valid distance reading.
 */
inline bool is_success(UsResult r)
{
    return r == UsResult::OK || r == UsResult::WEAK_SIGNAL;
}

/**
 * @brief Filter algorithm applied to the collected ping samples.
 */
enum class Filter
{
    MEDIAN,           /**< Selects the median value from a series of measurements. */
    DOMINANT_CLUSTER, /**< Finds the most frequent cluster of values and returns its average. */
};

/**
 * @brief Hardware and measurement configuration for the ultrasonic sensor.
 *
 * Note: ping_count is NOT here — it is passed as a parameter to read_distance()
 * so the application can vary it at runtime based on quality or error conditions.
 */
struct UsConfig
{
    uint16_t ping_interval_ms = 70; /**< Delay between consecutive pings in milliseconds. */
    uint16_t ping_duration_us = 20; /**< Duration of the trigger pulse in microseconds. */
    uint32_t timeout_us = 30000;    /**< Maximum wait time for an echo pulse in microseconds. */
    Filter filter = Filter::MEDIAN; /**< Statistical filter to apply to the measurements. */
    float min_distance_cm = 10.0f;  /**< The minimum valid distance in centimeters. */
    float max_distance_cm = 200.0f; /**< The maximum valid distance in centimeters. */
    float max_dev_cm = 15.0f;       /**< The maximum standard deviation allowed for a valid reading. */
    uint16_t warmup_time_ms = 600;  /**< The time to wait after initialization before the first measurement. */
};