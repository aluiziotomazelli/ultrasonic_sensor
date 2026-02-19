#pragma once

#include "us_types.hpp"
#include <cstdint>

/**
 * @interface IUsProcessor
 * @brief Interface for processing multiple ultrasonic ping samples into a final result.
 *
 * Receives an array of valid distance samples and the total pings attempted,
 * applies statistical filtering, and returns a unified UsResult.
 */
class IUsProcessor
{
public:
    // Maximum number of pings supported (matches UltrasonicConfig ping_count max)
    static constexpr uint8_t MAX_PINGS = 15;

    virtual ~IUsProcessor() = default;

    /**
     * @brief Processes an array of valid distance samples.
     *
     * @param raw_distances Array of valid distance samples (cm).
     * @param count         Number of valid samples in the array.
     * @param total_pings   Total pings attempted (used to compute valid ratio).
     * @param cfg           Configuration with variance and filter thresholds.
     * @return PingResult with unified UsResult and filtered distance.
     */
    virtual Reading process(const float *raw_distances, uint8_t count, uint8_t total_pings, const UsConfig &cfg) = 0;
};
