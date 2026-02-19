#include <gtest/gtest-param-test.h>
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <cmath>
#include <cstdint>

#include "esp_err.h"

#include "us_driver.hpp"
#include "us_types.hpp"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgReferee;

class MockGpioHAL : public IGpioHAL
{
public:
    MOCK_METHOD(esp_err_t, reset_pin, (gpio_num_t pin), (override));
    MOCK_METHOD(esp_err_t, config, (const gpio_config_t &config), (override));
    MOCK_METHOD(esp_err_t, set_level, (gpio_num_t pin, bool level), (override));
    MOCK_METHOD(esp_err_t, get_level, (gpio_num_t pin, bool &level), (override));
    MOCK_METHOD(esp_err_t, set_direction, (gpio_num_t pin, gpio_mode_t mode), (override));
    MOCK_METHOD(esp_err_t, set_drive_capability, (const gpio_num_t gpio_num, gpio_drive_cap_t strength), (override));
};

class MockTimerHAL : public ITimerHAL
{
public:
    MOCK_METHOD(uint64_t, get_now_us, (), (override));
    MOCK_METHOD(esp_err_t, delay_us, (uint32_t us), (override));
    MOCK_METHOD(esp_err_t, delay_ms, (uint32_t ms), (override));
};

class UsDriverTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        gpio_hal = std::make_shared<MockGpioHAL>();
        timer_hal = std::make_shared<MockTimerHAL>();
        driver = std::make_unique<UsDriver>(gpio_hal, timer_hal, TRIG_PIN, ECHO_PIN);
    }

    void ExpectPingPrepare()
    {
        // 1. Prepare
        EXPECT_CALL(*gpio_hal, set_direction(ECHO_PIN, GPIO_MODE_OUTPUT)).WillOnce(Return(ESP_OK));
        EXPECT_CALL(*gpio_hal, set_level(ECHO_PIN, 0)).WillOnce(Return(ESP_OK));
        EXPECT_CALL(*gpio_hal, set_direction(ECHO_PIN, GPIO_MODE_INPUT)).WillOnce(Return(ESP_OK));
    }

    void ExpectStuckCheck(bool is_stuck)
    {
        // 2. Stuck check
        EXPECT_CALL(*gpio_hal, get_level(ECHO_PIN, _)).WillOnce(DoAll(SetArgReferee<1>(is_stuck), Return(ESP_OK)));
    }

    void ExpectTriggerPulse(uint32_t duration_us)
    {
        // 3. Trigger pulse (line 121)
        EXPECT_CALL(*gpio_hal, set_level(TRIG_PIN, 1)).WillOnce(Return(ESP_OK));
        EXPECT_CALL(*timer_hal, delay_us(duration_us)).WillOnce(Return(ESP_OK));
        EXPECT_CALL(*gpio_hal, set_level(TRIG_PIN, 0)).WillOnce(Return(ESP_OK));
    }

    void ExpectRisingEdge(
        uint64_t start_time_us,
        uint32_t loops_until_high = 1, // How many loops until HIGH
        bool timeout = false)
    {
        // Initial timestamp
        EXPECT_CALL(*timer_hal, get_now_us()).WillOnce(Return(start_time_us));

        if (timeout) {
            // Simulates timeout: gpio always returns LOW
            EXPECT_CALL(*gpio_hal, get_level(ECHO_PIN, _))
                .WillRepeatedly(DoAll(SetArgReferee<1>(false), Return(ESP_OK)));

            // Timer advances beyond timeout
            EXPECT_CALL(*timer_hal, get_now_us()).WillRepeatedly(Return(start_time_us + 50000)); // > timeout_us
            return;
        }
        // 2. Simulates N loops with echo LOW
        for (uint32_t i = 0; i < loops_until_high - 1; i++) {
            EXPECT_CALL(*gpio_hal, get_level(ECHO_PIN, _)).WillOnce(DoAll(SetArgReferee<1>(false), Return(ESP_OK)));

            // Timer check inside the loop (not timeout yet)
            EXPECT_CALL(*timer_hal, get_now_us()).WillOnce(Return(start_time_us + (i + 1) * 10));
        }
        // 3. First HIGH (rising edge)
        EXPECT_CALL(*gpio_hal, get_level(ECHO_PIN, _)).WillOnce(DoAll(SetArgReferee<1>(true), Return(ESP_OK)));
        EXPECT_CALL(*timer_hal, get_now_us()).WillOnce(Return(start_time_us + loops_until_high * 10));
    }

    void ExpectEchoMeasurement(
        uint64_t echo_start_us,
        uint32_t pulse_duration_us,    // How long stays HIGH
        uint32_t loops_while_high = 3, // How many loops while HIGH
        bool timeout = false)
    {
        // 1. Timestamp do início da medição
        EXPECT_CALL(*timer_hal, get_now_us()).WillOnce(Return(echo_start_us));

        if (timeout) {
            // Simulates timeout: gpio always returns HIGH, never falls
            EXPECT_CALL(*gpio_hal, get_level(ECHO_PIN, _))
                .WillRepeatedly(DoAll(SetArgReferee<1>(true), Return(ESP_OK)));

            EXPECT_CALL(*timer_hal, get_now_us()).WillRepeatedly(Return(echo_start_us + 50000)); // > timeout
            return;
        }

        // 2. Simulates N loops with echo HIGH
        for (uint32_t i = 0; i < loops_while_high; i++) {
            EXPECT_CALL(*gpio_hal, get_level(ECHO_PIN, _)).WillOnce(DoAll(SetArgReferee<1>(true), Return(ESP_OK)));

            // Timer check (not timeout yet)
            EXPECT_CALL(*timer_hal, get_now_us()).WillOnce(Return(echo_start_us + (i + 1) * 100));
        }

        // 3. Last loop: echo goes to LOW (falling edge!)
        EXPECT_CALL(*gpio_hal, get_level(ECHO_PIN, _)).WillOnce(DoAll(SetArgReferee<1>(false), Return(ESP_OK)));

        // 4. Timer check final (not timeout)
        EXPECT_CALL(*timer_hal, get_now_us()).WillOnce(Return(echo_start_us + loops_while_high * 100));

        // 5. Timestamp final to calculate duration (line 213)
        EXPECT_CALL(*timer_hal, get_now_us()).WillOnce(Return(echo_start_us + pulse_duration_us));
    }

    void ExpectInterPingDelay(uint32_t delay_ms = 0)
    {
        EXPECT_CALL(*timer_hal, delay_ms(default_cfg_.ping_interval_ms)).WillOnce(Return(ESP_OK));
    }

    void PrepareTriger()
    {
        ExpectPingPrepare();
        ExpectStuckCheck(false);
    }

    static uint32_t distanceToPulseDuration(float cm)
    {
        return static_cast<uint32_t>((cm * 2.0f) / UsDriver::SOUND_SPEED_CM_PER_US);
    }

    void ExpectDistanceMeasurement(float target_cm)
    {
        uint32_t duration = distanceToPulseDuration(target_cm);
        ExpectEchoMeasurement(1010, duration);
    }

    void ExpectSuccessfulPing(uint32_t echo_duration_us = 1000)
    {
        ExpectPingPrepare();
        ExpectStuckCheck(false);
        ExpectTriggerPulse(20);
        ExpectRisingEdge(1000);
        ExpectEchoMeasurement(1010, echo_duration_us);
        ExpectInterPingDelay();
    }

    std::shared_ptr<MockGpioHAL> gpio_hal;
    std::shared_ptr<MockTimerHAL> timer_hal;
    std::unique_ptr<UsDriver> driver;
    UsConfig default_cfg_;

    const gpio_num_t TRIG_PIN = GPIO_NUM_4;
    const gpio_num_t ECHO_PIN = GPIO_NUM_5;
};

TEST_F(UsDriverTest, InitSuccess)
{
    EXPECT_CALL(*gpio_hal, reset_pin(_)).Times(2).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, config(_)).Times(2).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, set_level(_, 0)).Times(2).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, set_direction(ECHO_PIN, GPIO_MODE_OUTPUT)).WillOnce(Return(ESP_OK));

    EXPECT_EQ(ESP_OK, driver->init());
}

TEST_F(UsDriverTest, InitFailure)
{
    EXPECT_CALL(*gpio_hal, reset_pin(_)).WillOnce(Return(ESP_ERR_INVALID_ARG));
    EXPECT_EQ(ESP_ERR_INVALID_ARG, driver->init());

    EXPECT_CALL(*gpio_hal, reset_pin(_)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, config(_)).WillOnce(Return(ESP_ERR_INVALID_ARG));
    EXPECT_EQ(ESP_ERR_INVALID_ARG, driver->init());

    EXPECT_CALL(*gpio_hal, reset_pin(_)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, config(_)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, set_level(TRIG_PIN, 0)).WillOnce(Return(ESP_ERR_INVALID_ARG));
    EXPECT_EQ(ESP_ERR_INVALID_ARG, driver->init());

    EXPECT_CALL(*gpio_hal, reset_pin(_)).Times(2).WillOnce(Return(ESP_OK)).WillOnce(Return(ESP_ERR_INVALID_ARG));
    EXPECT_CALL(*gpio_hal, config(_)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, set_level(TRIG_PIN, 0)).WillOnce(Return(ESP_OK));
    EXPECT_EQ(ESP_ERR_INVALID_ARG, driver->init());

    EXPECT_CALL(*gpio_hal, reset_pin(_)).Times(2).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, config(_)).WillOnce(Return(ESP_OK)).WillOnce(Return(ESP_ERR_INVALID_ARG));
    EXPECT_CALL(*gpio_hal, set_level(TRIG_PIN, 0)).WillOnce(Return(ESP_OK));
    EXPECT_EQ(ESP_ERR_INVALID_ARG, driver->init());

    EXPECT_CALL(*gpio_hal, reset_pin(_)).Times(2).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, config(_)).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, set_level(_, 0)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, set_direction(ECHO_PIN, GPIO_MODE_OUTPUT)).WillOnce(Return(ESP_ERR_INVALID_ARG));
    EXPECT_EQ(ESP_ERR_INVALID_ARG, driver->init());

    EXPECT_CALL(*gpio_hal, reset_pin(_)).Times(2).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, config(_)).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, set_level(_, 0)).WillOnce(Return(ESP_OK)).WillOnce(Return(ESP_ERR_INVALID_ARG));
    EXPECT_CALL(*gpio_hal, set_direction(ECHO_PIN, _)).WillOnce(Return(ESP_OK));
    EXPECT_EQ(ESP_ERR_INVALID_ARG, driver->init());

    EXPECT_CALL(*gpio_hal, reset_pin(_)).Times(2).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, config(_)).Times(2).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, set_level(_, 0)).Times(2).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, set_direction(ECHO_PIN, _)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(*timer_hal, delay_ms(1000)).WillOnce(Return(ESP_ERR_TIMEOUT));
    EXPECT_EQ(ESP_ERR_TIMEOUT, driver->init(1000));
}

TEST_F(UsDriverTest, DeinitSuccess)
{
    EXPECT_CALL(*gpio_hal, set_level(_, 0)).Times(2).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, reset_pin(_)).Times(2).WillRepeatedly(Return(ESP_OK));
    EXPECT_EQ(ESP_OK, driver->deinit());
}

TEST_F(UsDriverTest, DeinitFailure)
{
    EXPECT_CALL(*gpio_hal, set_level(_, 0)).WillOnce(Return(ESP_ERR_INVALID_ARG));
    EXPECT_EQ(ESP_ERR_INVALID_ARG, driver->deinit());

    EXPECT_CALL(*gpio_hal, set_level(_, 0)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, reset_pin(_)).WillOnce(Return(ESP_ERR_INVALID_ARG));
    EXPECT_EQ(ESP_ERR_INVALID_ARG, driver->deinit());

    EXPECT_CALL(*gpio_hal, set_level(_, 0)).Times(2).WillOnce(Return(ESP_OK)).WillOnce(Return(ESP_ERR_INVALID_ARG));
    EXPECT_CALL(*gpio_hal, reset_pin(_)).WillOnce(Return(ESP_OK));
    EXPECT_EQ(ESP_ERR_INVALID_ARG, driver->deinit());

    EXPECT_CALL(*gpio_hal, set_level(_, 0)).Times(2).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, reset_pin(_)).Times(2).WillOnce(Return(ESP_OK)).WillOnce(Return(ESP_ERR_INVALID_ARG));
    EXPECT_EQ(ESP_ERR_INVALID_ARG, driver->deinit());
}

TEST_F(UsDriverTest, PingSuccess)
{
    UsConfig cfg;
    cfg.ping_duration_us = 20;
    cfg.timeout_us = 30000;

    uint64_t start_time_us = 1000;
    uint32_t echo_duration_us = 1000;

    InSequence s;

    // 1. Prepare (line 107)
    EXPECT_CALL(*gpio_hal, set_direction(ECHO_PIN, GPIO_MODE_OUTPUT)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, set_level(ECHO_PIN, 0)).WillOnce(Return(ESP_OK));

    // 2. switch echo (line 113)
    EXPECT_CALL(*gpio_hal, set_direction(ECHO_PIN, GPIO_MODE_INPUT)).WillOnce(Return(ESP_OK));

    // 2. Initial stuck check (line 117)
    EXPECT_CALL(*gpio_hal, get_level(ECHO_PIN, _)).WillOnce(DoAll(SetArgReferee<1>(false), Return(ESP_OK)));

    // 3. Trigger pulse (line 121)
    EXPECT_CALL(*gpio_hal, set_level(TRIG_PIN, 1)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(*timer_hal, delay_us(cfg.ping_duration_us)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, set_level(TRIG_PIN, 0)).WillOnce(Return(ESP_OK));

    // 4. Wait for rising edge (Loop 1: lines 182-197)
    EXPECT_CALL(*timer_hal, get_now_us()).WillOnce(Return(start_time_us)); // start mark (line 184)
    // Inside loop:
    EXPECT_CALL(*gpio_hal, get_level(ECHO_PIN, _))
        .WillOnce(DoAll(SetArgReferee<1>(true), Return(ESP_OK)));              // rising edge found
    EXPECT_CALL(*timer_hal, get_now_us()).WillOnce(Return(start_time_us + 1)); // timeout check (line 192)

    // 5. Measure high pulse (Loop 2: lines 199-216)
    uint64_t echo_start = start_time_us + 10;
    EXPECT_CALL(*timer_hal, get_now_us()).WillOnce(Return(echo_start)); // echo_start mark (line 201)
    // Inside loop:
    EXPECT_CALL(*gpio_hal, get_level(ECHO_PIN, _))
        .WillOnce(DoAll(SetArgReferee<1>(false), Return(ESP_OK)));          // falling edge found
    EXPECT_CALL(*timer_hal, get_now_us()).WillOnce(Return(echo_start + 1)); // timeout check (line 209)

    // 6. Echo ends (final duration calculation line 213)
    EXPECT_CALL(*timer_hal, get_now_us()).WillOnce(Return(echo_start + echo_duration_us)); // echo_end

    // 7. Inter-ping delay (line 152) applied inside ping_once
    EXPECT_CALL(*timer_hal, delay_ms(_)).WillOnce(Return(ESP_OK));

    auto result = driver->ping_once(cfg);
    EXPECT_EQ(result.result, UsResult::OK);
}

// Rising edge found immediately (1 loop)
TEST_F(UsDriverTest, RisingEdge_Simple)
{
    InSequence s;

    ExpectSuccessfulPing();

    auto result = driver->ping_once(default_cfg_);
    ASSERT_EQ(result, (Reading{UsResult::OK, 17.15f}));
}

// Rising edge found immediately (1 loop)
TEST_F(UsDriverTest, RisingEdge_ImmediateHigh)
{
    InSequence s;

    ExpectPingPrepare();
    ExpectStuckCheck(false);
    ExpectTriggerPulse(20);
    ExpectRisingEdge(1000, 1); // 1 loop and already HIGH
    ExpectEchoMeasurement(1010, 1000);
    ExpectInterPingDelay();

    auto result = driver->ping_once(default_cfg_);
    ASSERT_EQ(result, (Reading{UsResult::OK, 17.15f}));
}

// Rising edge after 5 loops (slow sensor)
TEST_F(UsDriverTest, RisingEdge_SlowResponse)
{
    InSequence s;

    ExpectPingPrepare();
    ExpectStuckCheck(false);
    ExpectTriggerPulse(20);
    ExpectRisingEdge(1000, 5); // 5 loops until HIGH
    ExpectEchoMeasurement(1050, 1000);
    ExpectInterPingDelay();

    auto result = driver->ping_once(default_cfg_);
    ASSERT_EQ(result, (Reading{UsResult::OK, 17.15f}));
}

// 1000us pulse (normal distance ~17cm)
TEST_F(UsDriverTest, EchoMeasurement_NormalDistance)
{
    InSequence s;

    ExpectPingPrepare();
    ExpectStuckCheck(false);
    ExpectTriggerPulse(20);
    ExpectRisingEdge(1000); // 5 loops until HIGH
    ExpectDistanceMeasurement(17.15f);
    ExpectInterPingDelay();

    auto result = driver->ping_once(default_cfg_);
    EXPECT_EQ(result.result, UsResult::OK);
    EXPECT_NEAR(result.cm, 17.15f, 0.5f);
}

// Pulse too long (max distance)
TEST_F(UsDriverTest, EchoMeasurement_MaxDistance)
{
    InSequence s;

    default_cfg_.max_distance_cm = 610;

    ExpectPingPrepare();
    ExpectStuckCheck(false);
    ExpectTriggerPulse(20);
    ExpectRisingEdge(1000);

    ExpectDistanceMeasurement(600.0f);
    ExpectInterPingDelay();

    auto result = driver->ping_once(default_cfg_);
    EXPECT_EQ(result.result, UsResult::OK);
    EXPECT_NEAR(result.cm, 600.0f, 5.0f);
}

TEST_F(UsDriverTest, EchoMeasurement_EdgeOfRangeDistance)
{
    InSequence s;

    default_cfg_.min_distance_cm = 10.0f;
    default_cfg_.max_distance_cm = 100.1f;

    ExpectPingPrepare();
    ExpectStuckCheck(false);
    ExpectTriggerPulse(20);
    ExpectRisingEdge(1000);

    ExpectDistanceMeasurement(10.1f);
    ExpectInterPingDelay();

    auto result = driver->ping_once(default_cfg_);
    EXPECT_EQ(result.result, UsResult::OK);
    EXPECT_NEAR(result.cm, 10.1f, 0.5f);

    ExpectPingPrepare();
    ExpectStuckCheck(false);
    ExpectTriggerPulse(20);
    ExpectRisingEdge(1000);

    ExpectDistanceMeasurement(100.0f);
    ExpectInterPingDelay();

    result = driver->ping_once(default_cfg_);
    EXPECT_EQ(result.result, UsResult::OK);
    EXPECT_NEAR(result.cm, 100.0f, 0.5f);
}

// ============================================================================
// Error cases
// ============================================================================

TEST_F(UsDriverTest, EchoMeasurement_OutOfRangeDistance)
{
    InSequence s;

    default_cfg_.min_distance_cm = 10.0f;
    default_cfg_.max_distance_cm = 100.0f;

    ExpectPingPrepare();
    ExpectStuckCheck(false);
    ExpectTriggerPulse(20);
    ExpectRisingEdge(1000);

    ExpectEchoMeasurement(1010, 400); // ← 0.4ms = ~6cm

    ASSERT_EQ(driver->ping_once(default_cfg_), (Reading{UsResult::OUT_OF_RANGE, 0.0f}));

    ExpectPingPrepare();
    ExpectStuckCheck(false);
    ExpectTriggerPulse(20);
    ExpectRisingEdge(1000);

    ExpectEchoMeasurement(1010, 35000); // ← 35ms = ~600cm

    ASSERT_EQ(driver->ping_once(default_cfg_), (Reading{UsResult::OUT_OF_RANGE, 0.0f}));
}

TEST_F(UsDriverTest, Ping_HardwareFault)
{
    InSequence s;

    EXPECT_CALL(*gpio_hal, set_direction(ECHO_PIN, GPIO_MODE_OUTPUT)).WillOnce(Return(ESP_FAIL));

    auto result = driver->ping_once(default_cfg_);
    EXPECT_EQ(UsResult::HW_FAULT, result.result);

    EXPECT_CALL(*gpio_hal, set_direction(ECHO_PIN, GPIO_MODE_OUTPUT)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, set_level(ECHO_PIN, 0)).WillOnce(Return(ESP_FAIL));

    result = driver->ping_once(default_cfg_);
    EXPECT_EQ(UsResult::HW_FAULT, result.result);

    EXPECT_CALL(*gpio_hal, set_direction(ECHO_PIN, GPIO_MODE_OUTPUT)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, set_level(ECHO_PIN, 0)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, set_direction(ECHO_PIN, GPIO_MODE_INPUT)).WillOnce(Return(ESP_FAIL));

    result = driver->ping_once(default_cfg_);
    EXPECT_EQ(UsResult::HW_FAULT, result.result);
}

TEST_F(UsDriverTest, Ping_PinStuckHigh)
{
    InSequence s;

    ExpectPingPrepare();
    ExpectStuckCheck(true);

    auto result = driver->ping_once(default_cfg_);
    EXPECT_EQ(UsResult::ECHO_STUCK, result.result);
}

TEST_F(UsDriverTest, Ping_TriggerFail)
{
    InSequence s;

    PrepareTriger();
    EXPECT_CALL(*gpio_hal, set_level(TRIG_PIN, 1)).WillOnce(Return(ESP_FAIL));

    auto result = driver->ping_once(default_cfg_);
    EXPECT_EQ(UsResult::HW_FAULT, result.result);

    PrepareTriger();
    EXPECT_CALL(*gpio_hal, set_level(TRIG_PIN, 1)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(*timer_hal, delay_us(20)).WillOnce(Return(ESP_FAIL));

    result = driver->ping_once(default_cfg_);
    EXPECT_EQ(UsResult::HW_FAULT, result.result);

    PrepareTriger();
    EXPECT_CALL(*gpio_hal, set_level(TRIG_PIN, 1)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(*timer_hal, delay_us(20)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(*gpio_hal, set_level(TRIG_PIN, 0)).WillOnce(Return(ESP_FAIL));

    result = driver->ping_once(default_cfg_);
    EXPECT_EQ(UsResult::HW_FAULT, result.result);
}

TEST_F(UsDriverTest, RaisingEdgeFails)
{
    InSequence s;

    ExpectPingPrepare();
    ExpectStuckCheck(false);
    ExpectTriggerPulse(20);
    ExpectRisingEdge(1000, 0, true); // timeout = true

    auto result = driver->ping_once(default_cfg_);
    EXPECT_EQ(UsResult::TIMEOUT, result.result);

    ExpectPingPrepare();
    ExpectStuckCheck(false);
    ExpectTriggerPulse(20);
    EXPECT_CALL(*timer_hal, get_now_us()).WillOnce(Return(1000));
    EXPECT_CALL(*gpio_hal, get_level(ECHO_PIN, _)).WillOnce(Return(ESP_FAIL));

    result = driver->ping_once(default_cfg_);
    EXPECT_EQ(UsResult::HW_FAULT, result.result);
}

// Timeout while measuring pulse (echo never falls)
TEST_F(UsDriverTest, MeasurePulseFails)
{
    InSequence s;

    ExpectPingPrepare();
    ExpectStuckCheck(false);
    ExpectTriggerPulse(20);
    ExpectRisingEdge(1000);
    ExpectEchoMeasurement(1010, 0, 0, true); // ← timeout

    auto result = driver->ping_once(default_cfg_);
    EXPECT_EQ(UsResult::TIMEOUT, result.result);

    ExpectPingPrepare();
    ExpectStuckCheck(false);
    ExpectTriggerPulse(20);
    ExpectRisingEdge(1000);
    EXPECT_CALL(*timer_hal, get_now_us()).WillOnce(Return(1000));
    EXPECT_CALL(*gpio_hal, get_level(ECHO_PIN, _)).WillOnce(Return(ESP_FAIL));

    result = driver->ping_once(default_cfg_);
    EXPECT_EQ(UsResult::HW_FAULT, result.result);
}

TEST_F(UsDriverTest, NoPingInterval)
{
    default_cfg_.ping_interval_ms = 0;

    InSequence s;

    ExpectPingPrepare();
    ExpectStuckCheck(false);
    ExpectTriggerPulse(20);
    ExpectRisingEdge(1000);

    ExpectDistanceMeasurement(20.0f);

    auto result = driver->ping_once(default_cfg_);
    EXPECT_EQ(result.result, UsResult::OK);
    EXPECT_NEAR(result.cm, 20.0f, 0.5f);
}
