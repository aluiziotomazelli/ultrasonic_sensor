#pragma once

#include <cmath>
#include <cstdint>

/**
 * @brief Unified result type for all ultrasonic sensor operations.
 */
enum class UsResult
{
    // Success — distance value is valid
    OK,          /**< Reliable reading, high ping ratio, low variance. */
    WEAK_SIGNAL, /**< Valid reading but ping ratio is below the ideal threshold. */

    // Logical failures — not enough valid data
    TIMEOUT,              /**< Sensor did not respond to trigger within timeout_us. */
    OUT_OF_RANGE,         /**< Measured distance is outside [min_distance_cm, max_distance_cm]. */
    HIGH_VARIANCE,        /**< Standard deviation of valid pings exceeds max_dev_cm. */
    INSUFFICIENT_SAMPLES, /**< Too few valid pings (ratio below minimum threshold). */

    // Hardware failures — require application-level action
    ECHO_STUCK, /**< ECHO pin is stuck HIGH. Application should power-cycle the sensor. */
    HW_FAULT,   /**< GPIO/HAL operation failed. */
};

/**
 * @brief Result of a single ping attempt or a full measurement cycle.
 */
struct Reading
{
    UsResult result; /**< Result status of the reading. */
    float cm;        /**< Distance in centimeters. Only valid if is_success(result). */

    /** @internal */
    bool operator==(const Reading &other) const
    {
        return (result == other.result) &&
               ((result != UsResult::OK && result != UsResult::WEAK_SIGNAL) || std::abs(cm - other.cm) < 0.001f);
    }

    /** @internal */
    bool operator!=(const Reading &other) const { return !(*this == other); }
};

/**
 * @brief Helper to check if a result represents a valid distance measurement.
 * @param r UsResult to check.
 * @return true if status is OK or WEAK_SIGNAL.
 */
inline bool is_success(UsResult r)
{
    return r == UsResult::OK || r == UsResult::WEAK_SIGNAL;
}

/**
 * @brief Statistical filter algorithm to apply to the collected ping samples.
 */
enum class Filter
{
    MEDIAN,           /**< Selects the median value from the sorted series. */
    DOMINANT_CLUSTER, /**< Averages the largest cluster of similar measurements. */
};

/**
 * @brief Configuration for the ultrasonic sensor hardware and processing.
 */
struct UsConfig
{
    uint16_t ping_interval_ms = 70; /**< Delay between consecutive pings (ms). */
    uint16_t ping_duration_us = 20; /**< Trigger pulse duration (us). */
    uint32_t timeout_us = 30000;    /**< Max wait for echo pulse (us). */
    Filter filter = Filter::MEDIAN; /**< Statistical filter type. */
    float min_distance_cm = 10.0f;  /**< Minimum valid distance (cm). */
    float max_distance_cm = 200.0f; /**< Maximum valid distance (cm). */
    float max_dev_cm = 15.0f;       /**< Max standard deviation for OK result (cm). */
    uint16_t warmup_time_ms = 600;  /**< Time to wait after init before first ping (ms). */
};
