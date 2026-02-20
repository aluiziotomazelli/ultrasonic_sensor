#include "us_processor.hpp"
#include "us_types.hpp"
#include <algorithm>
#include <vector>
#include "gtest/gtest.h"

TEST(UsProcessorTest, MedianFilter)
{
    UsProcessor processor;
    UsConfig cfg;
    cfg.filter = Filter::MEDIAN;

    // Samples should be within cfg.max_dev_cm (15.0f) to be OK
    Reading pings[] = {
        {UsResult::OK, 25.0f},
        {UsResult::OK, 35.0f},
        {UsResult::OK, 20.0f},
        {UsResult::OK, 40.0f},
        {UsResult::OK, 30.0f}
    };
    auto result = processor.process(pings, 5, cfg);

    EXPECT_EQ(UsResult::OK, result.result);
    EXPECT_FLOAT_EQ(30.0f, result.cm);
}

TEST(UsProcessorTest, ReduceMedianEmpty)
{
    UsProcessor processor;
    UsConfig cfg;
    cfg.filter = Filter::MEDIAN;

    auto result = processor.process(nullptr, 0, cfg);

    EXPECT_FLOAT_EQ(0.0f, result.cm);
    EXPECT_EQ(UsResult::INSUFFICIENT_SAMPLES, result.result);
}

TEST(UsProcessorTest, DominantClusterFilter)
{
    UsProcessor processor;
    UsConfig cfg;
    cfg.max_dev_cm = 200.0f; // Allow high variance to test filter's outlier rejection

    // Tank at 50cm, with outliers
    Reading pings[] = {
        {UsResult::OK, 50.1f},
        {UsResult::OK, 50.5f},
        {UsResult::OK, 49.8f},
        {UsResult::OK, 5.0f},
        {UsResult::OK, 50.2f},
        {UsResult::OK, 400.0f},
        {UsResult::OK, 49.9f}
    };
    auto result = processor.process(pings, 7, cfg);

    // Expected: average of (50.1, 50.5, 49.8, 50.2, 49.9) = 50.1
    // High variance in data (outliers) might lead to WEAK quality
    EXPECT_EQ(UsResult::WEAK_SIGNAL, result.result);
    EXPECT_NEAR(50.1f, result.cm, 0.1f);
}

TEST(UsProcessorTest, DominantClusterFilter_AllValid)
{
    UsProcessor processor;
    UsConfig cfg;
    cfg.filter = Filter::DOMINANT_CLUSTER;
    cfg.max_dev_cm = 200.0f;

    Reading pings[] = {
        {UsResult::OK, 50.1f},
        {UsResult::OK, 50.2f},
        {UsResult::OK, 49.9f},
        {UsResult::OK, 50.0f},
        {UsResult::OK, 50.3f}
    };
    auto result = processor.process(pings, 5, cfg);

    EXPECT_EQ(UsResult::OK, result.result);
    EXPECT_NEAR(50.1f, result.cm, 0.1f);
}

TEST(UsProcessorTest, DominantCluster_AllSame)
{
    UsProcessor processor;
    UsConfig cfg;
    cfg.filter = Filter::DOMINANT_CLUSTER;
    cfg.max_dev_cm = 200.0f;

    Reading pings[] = {
        {UsResult::OK, 50.0f},
        {UsResult::OK, 50.0f},
        {UsResult::OK, 50.0f},
        {UsResult::OK, 50.0f}
    };
    auto result = processor.process(pings, 4, cfg);

    EXPECT_FLOAT_EQ(50.0f, result.cm);
}

TEST(UsProcessorTest, DominantCluster_TwoClusters)
{
    UsProcessor processor;
    UsConfig cfg;
    cfg.filter = Filter::DOMINANT_CLUSTER;
    cfg.max_dev_cm = 200.0f;

    // Larger cluster: 50.x (5 elements)
    // Smaller cluster: 100.x (2 elements)
    Reading pings[] = {
        {UsResult::OK, 50.1f},
        {UsResult::OK, 50.5f},
        {UsResult::OK, 49.8f},
        {UsResult::OK, 100.0f},
        {UsResult::OK, 50.2f},
        {UsResult::OK, 100.5f},
        {UsResult::OK, 49.9f}
    };
    auto result = processor.process(pings, 7, cfg);

    // Average of dominant cluster should be ~50.1
    EXPECT_NEAR(50.1f, result.cm, 0.2f);
}

TEST(UsProcessorTest, DominantCluster_Empty)
{
    UsProcessor processor;
    UsConfig cfg;
    cfg.filter = Filter::DOMINANT_CLUSTER;

    // No valid samples, should fail by low ratio first
    auto result = processor.process(nullptr, 0, cfg);
    EXPECT_EQ(UsResult::INSUFFICIENT_SAMPLES, result.result);
}

TEST(UsProcessorTest, DominantCluster_NoCluster)
{
    UsProcessor processor;
    UsConfig cfg;
    cfg.filter = Filter::DOMINANT_CLUSTER;
    cfg.max_dev_cm = 200.0f;

    Reading pings[] = {
        {UsResult::OK, 10.0f},
        {UsResult::OK, 100.0f},
        {UsResult::OK, 200.0f},
        {UsResult::OK, 300.0f}
    };
    auto result = processor.process(pings, 4, cfg);

    // Should return median in fallback (200.0)
    EXPECT_FLOAT_EQ(200.0f, result.cm);
}

TEST(UsProcessorTest, LowPingRatio_Generic)
{
    UsProcessor processor;
    UsConfig cfg;

    Reading pings[] = {
        {UsResult::OK, 50.0f},
        {UsResult::OK, 50.1f},
        {UsResult::INSUFFICIENT_SAMPLES, 0.0f},
        {UsResult::INSUFFICIENT_SAMPLES, 0.0f},
        {UsResult::INSUFFICIENT_SAMPLES, 0.0f},
        {UsResult::INSUFFICIENT_SAMPLES, 0.0f},
        {UsResult::INSUFFICIENT_SAMPLES, 0.0f},
        {UsResult::INSUFFICIENT_SAMPLES, 0.0f},
        {UsResult::INSUFFICIENT_SAMPLES, 0.0f},
        {UsResult::INSUFFICIENT_SAMPLES, 0.0f}
    };
    auto result = processor.process(pings, 10, cfg); // ratio 0.2 < 0.4

    EXPECT_EQ(UsResult::INSUFFICIENT_SAMPLES, result.result);
}

TEST(UsProcessorTest, LowPingRatio_RefinedToOutOfRange)
{
    UsProcessor processor;
    UsConfig cfg;

    Reading pings[] = {
        {UsResult::OK, 50.0f},
        {UsResult::OUT_OF_RANGE, 0.0f},
        {UsResult::OUT_OF_RANGE, 0.0f},
        {UsResult::OUT_OF_RANGE, 0.0f},
        {UsResult::OUT_OF_RANGE, 0.0f}
    };
    auto result = processor.process(pings, 5, cfg); // ratio 0.2 < 0.4

    EXPECT_EQ(UsResult::OUT_OF_RANGE, result.result);
}

TEST(UsProcessorTest, LowPingRatio_RefinedToTimeout)
{
    UsProcessor processor;
    UsConfig cfg;

    Reading pings[] = {
        {UsResult::OK, 50.0f},
        {UsResult::TIMEOUT, 0.0f},
        {UsResult::TIMEOUT, 0.0f},
        {UsResult::TIMEOUT, 0.0f},
        {UsResult::TIMEOUT, 0.0f}
    };
    auto result = processor.process(pings, 5, cfg); // ratio 0.2 < 0.4

    EXPECT_EQ(UsResult::TIMEOUT, result.result);
}

TEST(UsProcessorTest, HighVariance)
{
    UsProcessor processor;
    UsConfig cfg;
    cfg.max_dev_cm = 5.0f;

    Reading pings[] = {
        {UsResult::OK, 10.0f},
        {UsResult::OK, 50.0f},
        {UsResult::OK, 10.0f},
        {UsResult::OK, 50.0f}
    };
    auto result = processor.process(pings, 4, cfg);

    EXPECT_EQ(UsResult::HIGH_VARIANCE, result.result);
}

TEST(UsProcessorTest, RatioBetweenThresholds_LowVariance)
{
    UsProcessor processor;
    UsConfig cfg;
    cfg.max_dev_cm = 15.0f;

    // ratio = 5/10 = 0.5 (between 0.4 and 0.7)
    Reading pings[] = {
        {UsResult::OK, 50.0f},
        {UsResult::OK, 50.1f},
        {UsResult::OK, 50.2f},
        {UsResult::OK, 49.9f},
        {UsResult::OK, 50.1f},
        {UsResult::TIMEOUT, 0.0f},
        {UsResult::TIMEOUT, 0.0f},
        {UsResult::TIMEOUT, 0.0f},
        {UsResult::TIMEOUT, 0.0f},
        {UsResult::TIMEOUT, 0.0f}
    };
    auto result = processor.process(pings, 10, cfg);

    EXPECT_EQ(UsResult::WEAK_SIGNAL, result.result); // Should be WEAK
}

TEST(UsProcessorTest, Boundaries_ExactlyAtThresholds)
{
    UsProcessor processor;
    UsConfig cfg;
    cfg.max_dev_cm = 15.0f;

    // Test 1: ratio == INVALID_PING_RATIO (0.4) - MUST be considered valid
    Reading pings_low[10] = {
        {UsResult::OK, 50.0f}, {UsResult::OK, 50.1f}, {UsResult::OK, 49.9f}, {UsResult::OK, 50.0f},
        {UsResult::TIMEOUT, 0.0f}, {UsResult::TIMEOUT, 0.0f}, {UsResult::TIMEOUT, 0.0f},
        {UsResult::TIMEOUT, 0.0f}, {UsResult::TIMEOUT, 0.0f}, {UsResult::TIMEOUT, 0.0f}
    };
    auto result_low = processor.process(pings_low, 10, cfg);
    EXPECT_NE(UsResult::INSUFFICIENT_SAMPLES, result_low.result);
    EXPECT_NE(UsResult::TIMEOUT, result_low.result);
    EXPECT_EQ(UsResult::WEAK_SIGNAL, result_low.result);

    // Test 2: ratio == VALID_PING_RATIO (0.7) - MUST be OK
    Reading pings_high[10] = {
        {UsResult::OK, 50.0f}, {UsResult::OK, 50.1f}, {UsResult::OK, 49.9f}, {UsResult::OK, 50.2f},
        {UsResult::OK, 49.8f}, {UsResult::OK, 50.1f}, {UsResult::OK, 50.0f},
        {UsResult::TIMEOUT, 0.0f}, {UsResult::TIMEOUT, 0.0f}, {UsResult::TIMEOUT, 0.0f}
    };
    auto result_high = processor.process(pings_high, 10, cfg);
    EXPECT_EQ(UsResult::OK, result_high.result);

    // Test 3: std_dev == max_dev_cm - MUST be valid (not INVALID)
    Reading pings_edge[2] = {{UsResult::OK, 0.0f}, {UsResult::OK, 10.0f}}; // std_dev = 5.0 exactly
    cfg.max_dev_cm = 5.0f;
    auto result_edge = processor.process(pings_edge, 2, cfg);
    EXPECT_NE(UsResult::INSUFFICIENT_SAMPLES, result_edge.result);
}
