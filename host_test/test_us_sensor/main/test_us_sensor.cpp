#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "esp_err.h"

#include "i_us_driver.hpp"
#include "us_processor.hpp"
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
        (const Reading *pings, uint8_t total_pings, const UsConfig &cfg),
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
 */
TEST_F(UsSensorTest, VerifySpecificConfigField)
{
    using ::testing::Field;

    Reading driver_reading = {UsResult::OK, 15.0f};
    Reading processed_reading = {UsResult::OK, 15.0f};

    EXPECT_CALL(*driver, ping_once(Field(&UsConfig::timeout_us, cfg_.timeout_us))).WillOnce(Return(driver_reading));

    EXPECT_CALL(*processor, process(_, 1, Field(&UsConfig::filter, cfg_.filter)))
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

TEST_F(UsSensorTest, ReadingHappyPath)
{
    Reading driver_reading = {UsResult::OK, 10.0f};
    Reading processed_reading = {UsResult::OK, 10.0f};

    EXPECT_CALL(*driver, ping_once(_)).WillOnce(Return(driver_reading));
    EXPECT_CALL(*processor, process(_, 1, _)).WillOnce(Return(processed_reading));

    auto result = sensor->read_distance(1);
    ASSERT_EQ(result, processed_reading);
}

TEST_F(UsSensorTest, ReadingMultiplesPings)
{
    Reading driver_reading = {UsResult::OK, 10.0f};
    Reading processed_reading = {UsResult::OK, 10.0f};

    EXPECT_CALL(*driver, ping_once(_)).WillRepeatedly(Return(driver_reading));
    EXPECT_CALL(*processor, process(_, 10, _)).WillOnce(Return(processed_reading));

    auto result = sensor->read_distance(10);
    ASSERT_EQ(result, processed_reading);
}

TEST_F(UsSensorTest, ReadingClampingPingCount)
{
    Reading driver_reading = {UsResult::OK, 10.0f};
    Reading processed_reading = {UsResult::OK, 10.0f};

    EXPECT_CALL(*driver, ping_once(_)).WillRepeatedly(Return(driver_reading));
    EXPECT_CALL(*processor, process(_, 1, _)).WillOnce(Return(processed_reading));

    auto result = sensor->read_distance(0);
    ASSERT_EQ(result, processed_reading);

    EXPECT_CALL(*processor, process(_, IUsProcessor::MAX_PINGS, _))
        .WillOnce(Return(processed_reading));
    result = sensor->read_distance(IUsProcessor::MAX_PINGS + 1);
    ASSERT_EQ(result, processed_reading);
}

TEST_F(UsSensorTest, ECHO_STUCK_HW_FAULT_Failure)
{
    Reading driver_reading = {UsResult::ECHO_STUCK, 10.0f};
    Reading processed_reading = {UsResult::ECHO_STUCK, 0.0f};

    EXPECT_CALL(*driver, ping_once(_)).WillOnce(Return(driver_reading));

    auto result = sensor->read_distance(1);
    ASSERT_EQ(result, processed_reading);

    driver_reading = {UsResult::HW_FAULT, 10.0f};
    processed_reading = {UsResult::HW_FAULT, 0.0f};

    EXPECT_CALL(*driver, ping_once(_)).WillOnce(Return(driver_reading));

    result = sensor->read_distance(1);
    ASSERT_EQ(result, processed_reading);
}

TEST_F(UsSensorTest, LogicalFailuresPassedToProcessor)
{
    // OUT_OF_RANGE should be passed to processor
    Reading driver_reading = {UsResult::OUT_OF_RANGE, 0.0f};
    Reading processed_reading = {UsResult::OUT_OF_RANGE, 0.0f};

    EXPECT_CALL(*driver, ping_once(_)).WillOnce(Return(driver_reading));
    EXPECT_CALL(*processor, process(_, 1, _)).WillOnce(Return(processed_reading));

    auto result = sensor->read_distance(1);
    ASSERT_EQ(result, processed_reading);

    // TIMEOUT should be passed to processor
    driver_reading = {UsResult::TIMEOUT, 0.0f};
    processed_reading = {UsResult::TIMEOUT, 0.0f};

    EXPECT_CALL(*driver, ping_once(_)).WillOnce(Return(driver_reading));
    EXPECT_CALL(*processor, process(_, 1, _)).WillOnce(Return(processed_reading));

    result = sensor->read_distance(1);
    ASSERT_EQ(result, processed_reading);
}

TEST_F(UsSensorTest, IntegrationWithRealProcessor_OutOfRange)
{
    // Use real processor to verify the fix
    auto real_processor = std::make_shared<UsProcessor>();
    UsSensor sensor_int(cfg_, driver, real_processor);

    Reading driver_reading = {UsResult::OUT_OF_RANGE, 0.0f};

    // Majority of pings fail with OUT_OF_RANGE
    EXPECT_CALL(*driver, ping_once(_)).WillRepeatedly(Return(driver_reading));

    auto result = sensor_int.read_distance(5);
    ASSERT_EQ(result.result, UsResult::OUT_OF_RANGE);
}

TEST_F(UsSensorTest, IntegrationWithRealProcessor_Timeout)
{
    auto real_processor = std::make_shared<UsProcessor>();
    UsSensor sensor_int(cfg_, driver, real_processor);

    Reading driver_reading = {UsResult::TIMEOUT, 0.0f};

    EXPECT_CALL(*driver, ping_once(_)).WillRepeatedly(Return(driver_reading));

    auto result = sensor_int.read_distance(5);
    ASSERT_EQ(result.result, UsResult::TIMEOUT);
}

TEST(UsSensorIntegrationTest, FactoryConstructorCreatesRealObjects)
{
    UsConfig cfg;
    UsSensor sensor(GPIO_NUM_4, GPIO_NUM_5, cfg);
    EXPECT_TRUE(true);
}
