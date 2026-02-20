#pragma once

#include "i_us_processor.hpp"
#include "us_types.hpp"
#include <cstddef>

/**
 * @brief Concrete implementation of IUsProcessor for statistical filtering of ultrasonic samples.
 * @internal
 */
class UsProcessor : public IUsProcessor
{
public:
    /** @internal */
    UsProcessor() = default;
    ~UsProcessor() override = default;

    /** @copydoc IUsProcessor::process() */
    Reading process(const Reading *pings, uint8_t total_pings, const UsConfig &cfg) override;

private:
    /** @internal */
    float reduce_median(float *v, std::size_t n);

    /** @internal */
    float reduce_dominant_cluster(float *v, std::size_t n);

    /** @internal */
    float get_std_dev(const float *samples, uint8_t count);
};
