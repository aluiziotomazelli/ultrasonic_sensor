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
     * @brief Processes an array of ping results.
     *
     * @param pings       Array of Reading structures from driver pings.
     * @param total_pings Total pings attempted (used to compute valid ratio).
     * @param cfg         Configuration with variance and filter thresholds.
     * @return Reading with unified UsResult and filtered distance.
     */
    virtual Reading process(const Reading *pings, uint8_t total_pings, const UsConfig &cfg) = 0;
};
