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
    float samples[] = {25.0f, 35.0f, 20.0f, 40.0f, 30.0f}; // Sorted: 20, 25, 30, 35, 40. StdDev ~7.07
    auto result = processor.process(samples, 5, 5, cfg);

    EXPECT_EQ(UsResult::OK, result.result);
    EXPECT_FLOAT_EQ(30.0f, result.cm);
}

TEST(UsProcessorTest, ReduceMedianEmpty)
{
    UsProcessor processor;
    UsConfig cfg;
    cfg.filter = Filter::MEDIAN;

    float samples[] = {};
    auto result = processor.process(samples, 0, 0, cfg);

    EXPECT_FLOAT_EQ(0.0f, result.cm);
    EXPECT_EQ(UsResult::INSUFFICIENT_SAMPLES, result.result);
}

TEST(UsProcessorTest, DominantClusterFilter)
{
    UsProcessor processor;
    UsConfig cfg;
    cfg.max_dev_cm = 200.0f; // Allow high variance to test filter's outlier rejection

    // Tank at 50cm, with outliers
    float samples[] = {50.1f, 50.5f, 49.8f, 5.0f, 50.2f, 400.0f, 49.9f};
    auto result = processor.process(samples, 7, 7, cfg);

    // Expected: average of (50.1, 50.5, 49.8, 50.2, 49.9) = 50.1
    // High variance in data (outliers) might lead to WEAK quality
    EXPECT_EQ(UsResult::WEAK_SIGNAL, result.result);
    EXPECT_NEAR(50.1f, result.cm, 0.1f);
}

TEST(UsProcessorTest, DominantClusterFilter_Explicit)
{
    UsProcessor processor;
    UsConfig cfg;
    cfg.filter = Filter::DOMINANT_CLUSTER; // EXPLICIT
    cfg.max_dev_cm = 200.0f;               // Allow high variance to test filter's outlier rejection

    float samples[] = {50.1f, 50.5f, 49.8f, 5.0f, 50.2f, 400.0f, 49.9f};
    auto result = processor.process(samples, 7, 7, cfg);

    EXPECT_EQ(UsResult::WEAK_SIGNAL, result.result); // Still WEAK due to outliers
    EXPECT_NEAR(50.1f, result.cm, 0.1f);
}

TEST(UsProcessorTest, DominantClusterFilter_AllValid)
{
    UsProcessor processor;
    UsConfig cfg;
    cfg.filter = Filter::DOMINANT_CLUSTER;
    cfg.max_dev_cm = 200.0f;

    float samples[] = {50.1f, 50.2f, 49.9f, 50.0f, 50.3f};
    auto result = processor.process(samples, 5, 5, cfg);

    EXPECT_EQ(UsResult::OK, result.result);
    EXPECT_NEAR(50.1f, result.cm, 0.1f);
}

TEST(UsProcessorTest, DominantCluster_AllSame)
{
    UsProcessor processor;
    UsConfig cfg;
    cfg.filter = Filter::DOMINANT_CLUSTER;
    cfg.max_dev_cm = 200.0f; // Desabilita validação

    float samples[] = {50.0f, 50.0f, 50.0f, 50.0f};
    auto result = processor.process(samples, 4, 4, cfg);

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
    float samples[] = {50.1f, 50.5f, 49.8f, 100.0f, 50.2f, 100.5f, 49.9f};
    auto result = processor.process(samples, 7, 7, cfg);

    // Average of dominant cluster should be ~50.1
    EXPECT_NEAR(50.1f, result.cm, 0.2f);
}

TEST(UsProcessorTest, DominantCluster_Empty)
{
    UsProcessor processor;
    UsConfig cfg;
    cfg.filter = Filter::DOMINANT_CLUSTER;

    // No valid samples, should fail by low ratio first
    auto result = processor.process(nullptr, 0, 5, cfg);
    EXPECT_EQ(UsResult::INSUFFICIENT_SAMPLES, result.result);
    EXPECT_FLOAT_EQ(0.0f, result.cm);
}

TEST(UsProcessorTest, DominantCluster_NoCluster)
{
    UsProcessor processor;
    UsConfig cfg;
    cfg.filter = Filter::DOMINANT_CLUSTER;
    cfg.max_dev_cm = 200.0f;

    float samples[] = {10.0f, 100.0f, 200.0f, 300.0f}; // All > DELTA_CM (5.0) apart
    auto result = processor.process(samples, 4, 4, cfg);

    // Reduce median
    std::sort(samples, samples + 4);
    float median = samples[4 / 2];
    // Should return median in fallback
    EXPECT_FLOAT_EQ(median, result.cm);
}

TEST(UsProcessorTest, LowPingRatio)
{
    UsProcessor processor;
    UsConfig cfg;

    float samples[] = {50.0f, 50.1f};
    auto result = processor.process(samples, 2, 10, cfg); // ratio 0.2 < 0.4

    EXPECT_EQ(UsResult::INSUFFICIENT_SAMPLES, result.result);
}

TEST(UsProcessorTest, TotalPingsZero)
{
    UsProcessor processor;
    UsConfig cfg;
    auto result = processor.process(nullptr, 0, 0, cfg);
    EXPECT_EQ(UsResult::INSUFFICIENT_SAMPLES, result.result);
}

TEST(UsProcessorTest, HighVariance)
{
    UsProcessor processor;
    UsConfig cfg;
    cfg.max_dev_cm = 5.0f;

    float samples[] = {10.0f, 50.0f, 10.0f, 50.0f}; // StdDev is 20.0 > 5.0
    auto result = processor.process(samples, 4, 4, cfg);

    EXPECT_EQ(UsResult::HIGH_VARIANCE, result.result);
}

TEST(UsProcessorTest, RatioBetweenThresholds_LowVariance)
{
    UsProcessor processor;
    UsConfig cfg;
    cfg.max_dev_cm = 15.0f;

    // ratio = 5/10 = 0.5 (between 0.4 and 0.7)
    float samples[] = {50.0f, 50.1f, 50.2f, 49.9f, 50.1f}; // 5 válidos
    auto result = processor.process(samples, 5, 10, cfg);

    EXPECT_EQ(UsResult::WEAK_SIGNAL, result.result); // Should be WEAK
}

TEST(UsProcessorTest, Boundaries_ExactlyAtThresholds)
{
    UsProcessor processor;
    UsConfig cfg;
    cfg.max_dev_cm = 15.0f;

    // Test 1: ratio == INVALID_PING_RATIO (0.4) - MUST be considered valid
    float samples_low[4] = {50.0f, 50.1f, 49.9f, 50.0f}; // 4 samples
    auto result_low = processor.process(samples_low, 4, 10, cfg);
    EXPECT_NE(UsResult::INSUFFICIENT_SAMPLES, result_low.result); // Should not be INVALID
    EXPECT_EQ(UsResult::WEAK_SIGNAL, result_low.result);          // Should be WEAK

    // Test 2: ratio == VALID_PING_RATIO (0.7) - MUST be OK
    float samples_high[7] = {50.0f, 50.1f, 49.9f, 50.2f, 49.8f, 50.1f, 50.0f};
    auto result_high = processor.process(samples_high, 7, 10, cfg);
    EXPECT_EQ(UsResult::OK, result_high.result);

    // Test 3: std_dev == max_dev_cm - MUST be valid (not INVALID)
    float samples_edge[2] = {0.0f, 10.0f}; // std_dev = 5.0 exactly
    cfg.max_dev_cm = 5.0f;
    auto result_edge = processor.process(samples_edge, 2, 2, cfg);
    EXPECT_NE(UsResult::INSUFFICIENT_SAMPLES, result_edge.result);
}

TEST(UsProcessorTest, StdDevExactlyAtLimit)
{
    UsProcessor processor;
    UsConfig cfg;
    cfg.max_dev_cm = 5.0f;

    // std_dev == max_dev_cm
    float samples[] = {0.0f, 10.0f}; // std_dev = 5.0
    auto result = processor.process(samples, 2, 2, cfg);

    // Should be OK (not INVALID)
    EXPECT_NE(UsResult::INSUFFICIENT_SAMPLES, result.result);
}