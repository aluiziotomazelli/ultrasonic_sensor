#pragma once

#include "i_us_processor.hpp"
#include "us_types.hpp"
#include <cstddef>

/**
 * @class UsProcessor
 * @brief Concrete implementation of IUsProcessor for statistical filtering of ultrasonic samples.
 */
class UsProcessor : public IUsProcessor
{
public:
    UsProcessor() = default;
    ~UsProcessor() override = default;

    /** @copydoc IUsProcessor::process() */
    Reading process(const Reading *pings, uint8_t total_pings, const UsConfig &cfg) override;

private:
    /** @brief Selects the median value from a series of measurements. */
    float reduce_median(float *v, std::size_t n);

    /** @brief Finds the most frequent cluster of values and returns its average. */
    float reduce_dominant_cluster(float *v, std::size_t n);

    /** @brief Calculates the standard deviation of valid measurements. */
    float get_std_dev(const float *samples, uint8_t count);
};
