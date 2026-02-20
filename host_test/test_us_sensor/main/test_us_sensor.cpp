#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "esp_err.h"

#include "i_us_driver.hpp"
#include "us_sensor.hpp"
#include "us_types.hpp"

using ::testing::_;
using ::testing::Return;

class MockUsDriver : public IUsDriver
{
public:
    MOCK_METHOD(esp_err_t, init, (uint16_t warmup_time_ms), (override));
    MOCK_METHOD(esp_err_t, deinit, (), (override));
    MOCK_METHOD(Reading, ping_once, (const UsConfig &cfg), (override));
};

class MockUsProcessor : public IUsProcessor
{
public:
    MOCK_METHOD(
        Reading,
        process,
        (const float *raw_distances, uint8_t count, uint8_t total_pings, const UsConfig &cfg),
        (override));
};

class UsSensorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        driver = std::make_shared<MockUsDriver>();
        processor = std::make_shared<MockUsProcessor>();
        sensor = std::make_unique<UsSensor>(cfg_, driver, processor);
    }

    std::shared_ptr<MockUsDriver> driver;
    std::shared_ptr<MockUsProcessor> processor;
    std::unique_ptr<UsSensor> sensor;
    UsConfig cfg_;

    const gpio_num_t TRIG_PIN = GPIO_NUM_4;
    const gpio_num_t ECHO_PIN = GPIO_NUM_5;
};

/**
 * @brief Example of granular verification using GMock Field matcher.
 * This allows us to verify specific members of a struct without requiring operator== in the production header.
 */
TEST_F(UsSensorTest, VerifySpecificConfigField)
{
    using ::testing::Field;

    Reading driver_reading = {UsResult::OK, 15.0f};
    Reading processed_reading = {UsResult::OK, 15.0f};

    // We can explicitly verify that a specific field (e.g., timeout_us) is passed correctly
    // while ignoring other fields in the UsConfig struct.
    EXPECT_CALL(*driver, ping_once(Field(&UsConfig::timeout_us, cfg_.timeout_us))).WillOnce(Return(driver_reading));

    // Also verify that the configuration is passed correctly to the processor
    EXPECT_CALL(*processor, process(_, _, _, Field(&UsConfig::filter, cfg_.filter)))
        .WillOnce(Return(processed_reading));

    auto result = sensor->read_distance(1);
    ASSERT_EQ(result, processed_reading);
}

// =================================================================
// Init/deinit tests
// =================================================================

TEST_F(UsSensorTest, InitCallsDriverInit)
{
    EXPECT_CALL(*driver, init(cfg_.warmup_time_ms)).WillOnce(Return(ESP_OK));
    ASSERT_EQ(sensor->init(), ESP_OK);
}

TEST_F(UsSensorTest, DeinitCallsDriverDeinit)
{
    EXPECT_CALL(*driver, deinit()).WillOnce(Return(ESP_OK));
    ASSERT_EQ(sensor->deinit(), ESP_OK);
}

// ==================================================================
// read_distance(uint8_t ping_count)
// ==================================================================

/**
 * @brief Standard success case: One ping requested, one successful driver reading,
 * and successful statistical processing.
 */
TEST_F(UsSensorTest, ReadingHappyPath)
{
    // Define the expected intermediate and final results
    Reading driver_reading = {UsResult::OK, 10.0f};
    Reading processed_reading = {UsResult::OK, 10.0f};

    /**
     * Set expectations for dependencies:
     *
     * 1. The sensor should call driver->ping_once() exactly once.
     *    We use '_' as a wildcard matcher because we don't need to verify
     *    the specific configuration values in this test case.
     */
    EXPECT_CALL(*driver, ping_once(_)).WillOnce(Return(driver_reading));

    /**
     * 2. The sensor should then call processor->process().
     *    - The first argument is the internal samples array (we ignore it with '_').
     *    - The second is the number of valid samples (1).
     *    - The third is the total pings requested (1).
     *    - The fourth is the configuration struct (ignored with '_').
     */
    EXPECT_CALL(*processor, process(_, 1, 1, _)).WillOnce(Return(processed_reading));

    /**
     * Execute the operation and verify the final result.
     * ASSERT_EQ uses the operator== we added to the Reading struct,
     * allowing for a concise, whole-object comparison.
     */
    auto result = sensor->read_distance(1);
    ASSERT_EQ(result, processed_reading);
}

TEST_F(UsSensorTest, ReadingMultiplesPings)
{
    // Define the expected intermediate and final results
    Reading driver_reading = {UsResult::OK, 10.0f};
    Reading processed_reading = {UsResult::OK, 10.0f};

    EXPECT_CALL(*driver, ping_once(_)).WillRepeatedly(Return(driver_reading));
    EXPECT_CALL(*processor, process(_, _, _, _)).WillOnce(Return(processed_reading));

    auto result = sensor->read_distance(10);
    ASSERT_EQ(result, processed_reading);
}

TEST_F(UsSensorTest, ReadingClampingPingCount)
{
    // Define the expected intermediate and final results
    Reading driver_reading = {UsResult::OK, 10.0f};
    Reading processed_reading = {UsResult::OK, 10.0f};

    EXPECT_CALL(*driver, ping_once(_)).WillRepeatedly(Return(driver_reading));
    // Expecting 1 ping because 0 is clamped to 1
    EXPECT_CALL(*processor, process(_, 1, 1, _)).WillOnce(Return(processed_reading));

    // Reading with 0 pings will be processed as 1 ping
    auto result = sensor->read_distance(0);
    // Expecting the same result as reading with 1 ping
    ASSERT_EQ(result, processed_reading);

    // Reading with more than MAX_PINGS will be processed as MAX_PINGS
    EXPECT_CALL(*processor, process(_, IUsProcessor::MAX_PINGS, IUsProcessor::MAX_PINGS, _))
        .WillOnce(Return(processed_reading));
    result = sensor->read_distance(IUsProcessor::MAX_PINGS + 1);
    // Expecting the same result as reading with MAX_PINGS
    ASSERT_EQ(result, processed_reading);
}

TEST_F(UsSensorTest, ECHO_STUCK_HW_FAULT_Failure)
{
    // Define the expected intermediate result
    Reading driver_reading = {UsResult::ECHO_STUCK, 10.0f};
    // With ECHO_STUCK, the processor is NOT invoked, so should return 0.0f
    Reading processed_reading = {UsResult::ECHO_STUCK, 0.0f};

    // Expect driver to return ECHO_STUCK
    EXPECT_CALL(*driver, ping_once(_)).WillOnce(Return(driver_reading));

    // Read distance should return ECHO_STUCK
    auto result = sensor->read_distance(1);
    ASSERT_EQ(result, processed_reading);

    driver_reading = {UsResult::HW_FAULT, 10.0f};
    processed_reading = {UsResult::HW_FAULT, 0.0f};

    // Expect driver to return HW_FAULT
    EXPECT_CALL(*driver, ping_once(_)).WillOnce(Return(driver_reading));

    result = sensor->read_distance(1);
    ASSERT_EQ(result, processed_reading);
}

TEST_F(UsSensorTest, OUT_OF_RANGE_Failure)
{
    using ::testing::Field;

    // Define the distance that will trigger OUT_OF_RANGE
    Reading driver_reading = {UsResult::OUT_OF_RANGE, (cfg_.min_distance_cm - 1.0f)};
    // With OUT_OF_RANGE, the processor should return 0.0f
    Reading processed_reading = {UsResult::OUT_OF_RANGE, 0.0f};

    // Minimum distance has default value of 10 cm
    // cfg_.min_distance_cm = 10;

    // Verify that the configuration is passed correctly to the driver
    EXPECT_CALL(*driver, ping_once(_)).WillOnce(Return(driver_reading));

    // - The first argument is the internal samples array (we ignore it with '_').
    // - The second: number of valid samples (0). If the distance is out of range, will be 0.
    // - The third: total pings requested (1).
    // - The fourth: configuration struct (ignored with '_').
    EXPECT_CALL(*processor, process(_, 0, 1, _)).WillOnce(Return(processed_reading));

    auto result = sensor->read_distance(1);
    ASSERT_EQ(result, processed_reading);

    driver_reading = {UsResult::OUT_OF_RANGE, (cfg_.max_distance_cm + 1.0f)};
    processed_reading = {UsResult::OUT_OF_RANGE, 0.0f};

    EXPECT_CALL(*driver, ping_once(_)).WillOnce(Return(driver_reading));
    EXPECT_CALL(*processor, process(_, 0, 1, _)).WillOnce(Return(processed_reading));

    result = sensor->read_distance(1);
    ASSERT_EQ(result, processed_reading);
}

TEST_F(UsSensorTest, EdgeRangeCases)
{
    // Test minimum distance exactly
    Reading driver_reading = {UsResult::OK, (cfg_.min_distance_cm)};
    // Sould return the same value
    Reading processed_reading = {UsResult::OK, cfg_.min_distance_cm};

    EXPECT_CALL(*driver, ping_once(_)).WillOnce(Return(driver_reading));

    // - The second: number of valid samples needs match 1 as it will be 1 valid sample
    EXPECT_CALL(*processor, process(_, 1, 1, _)).WillOnce(Return(processed_reading));

    auto result = sensor->read_distance(1);
    ASSERT_EQ(result, processed_reading);

    // Test maximum distance exactly
    driver_reading = {UsResult::OK, (cfg_.max_distance_cm)};
    processed_reading = {UsResult::OK, cfg_.max_distance_cm};

    EXPECT_CALL(*driver, ping_once(_)).WillOnce(Return(driver_reading));
    EXPECT_CALL(*processor, process(_, 1, 1, _)).WillOnce(Return(processed_reading));

    result = sensor->read_distance(1);
    ASSERT_EQ(result, processed_reading);
}

TEST_F(UsSensorTest, TIMEOUT_Failure)
{
    using ::testing::Field;

    // Define TIMEOUT result on reading
    Reading driver_reading = {UsResult::TIMEOUT, 0.0f};
    // With TIMEOUT, the processor should return 0.0f
    Reading processed_reading = {UsResult::TIMEOUT, 0.0f};

    // Verify that the configuration is passed correctly to the driver
    EXPECT_CALL(*driver, ping_once(_)).WillOnce(Return(driver_reading));

    // - The second: number of valid samples (0). If the distance is out of range, will be 0.
    // - The third: total pings requested (1).
    EXPECT_CALL(*processor, process(_, 0, 1, _)).WillOnce(Return(processed_reading));

    auto result = sensor->read_distance(1);
    ASSERT_EQ(result, processed_reading);
}

TEST_F(UsSensorTest, InsufficientSamplesRefinedToOutOfRange)
{
    Reading driver_reading = {UsResult::OUT_OF_RANGE, 0.0f};
    Reading processor_result = {UsResult::INSUFFICIENT_SAMPLES, 0.0f};
    Reading expected_final = {UsResult::OUT_OF_RANGE, 0.0f};

    EXPECT_CALL(*driver, ping_once(_)).WillRepeatedly(Return(driver_reading));
    EXPECT_CALL(*processor, process(_, 0, 5, _)).WillOnce(Return(processor_result));

    auto result = sensor->read_distance(5);
    ASSERT_EQ(result, expected_final);
}

TEST_F(UsSensorTest, InsufficientSamplesRefinedToTimeout)
{
    Reading driver_reading = {UsResult::TIMEOUT, 0.0f};
    Reading processor_result = {UsResult::INSUFFICIENT_SAMPLES, 0.0f};
    Reading expected_final = {UsResult::TIMEOUT, 0.0f};

    EXPECT_CALL(*driver, ping_once(_)).WillRepeatedly(Return(driver_reading));
    EXPECT_CALL(*processor, process(_, 0, 5, _)).WillOnce(Return(processor_result));

    auto result = sensor->read_distance(5);
    ASSERT_EQ(result, expected_final);
}

TEST(UsSensorIntegrationTest, FactoryConstructorCreatesRealObjects)
{
    UsConfig cfg;

    UsSensor sensor(GPIO_NUM_4, GPIO_NUM_5, cfg); // Testing if factory constructor is not crashing
    EXPECT_TRUE(true);
}