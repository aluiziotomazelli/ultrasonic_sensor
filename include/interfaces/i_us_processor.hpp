#pragma once

#include "us_types.hpp"
#include <cstdint>

/**
 * @brief Interface for processing multiple ultrasonic ping samples into a final result.
 * @internal
 *
 * Applies statistical filtering and evaluates signal quality.
 */
class IUsProcessor
{
public:
    /** @internal */
    static constexpr uint8_t MAX_PINGS = 15;

    virtual ~IUsProcessor() = default;

    /** @internal */
    virtual Reading process(const Reading *pings, uint8_t total_pings, const UsConfig &cfg) = 0;
};
