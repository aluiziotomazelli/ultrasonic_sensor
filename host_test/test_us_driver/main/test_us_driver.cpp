// components/ultrasonic_sensor/host_test/test_us_driver/main/test_us_driver.cpp

#include <gtest/gtest-param-test.h>
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <cmath>
#include <cstdint>

#include "esp_err.h"

#include "mock_hal_gpio.hpp"
#include "mock_hal_timer.hpp"
#include "mock_hal_sys_rom.hpp"
#include "us_driver.hpp"
#include "us_types.hpp"

using namespace ultrasonic;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgReferee;

class UsDriverTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        driver = std::make_unique<UsDriver>(gpio_hal, timer_hal, sys_rom_hal, TRIG_PIN, ECHO_PIN);
    }

    void ExpectPingPrepare()
    {
        // 1. Prepare
        EXPECT_CALL(gpio_hal, set_direction(ECHO_PIN, GPIO_MODE_OUTPUT)).WillOnce(Return(ESP_OK));
        EXPECT_CALL(gpio_hal, set_level(ECHO_PIN, 0)).WillOnce(Return(ESP_OK));
        EXPECT_CALL(gpio_hal, set_direction(ECHO_PIN, GPIO_MODE_INPUT)).WillOnce(Return(ESP_OK));
    }

    void ExpectStuckCheck(bool is_stuck)
    {
        // 2. Stuck check
        EXPECT_CALL(gpio_hal, get_level(ECHO_PIN)).WillOnce(Return(is_stuck ? 1 : 0));
    }

    void ExpectTriggerPulse(uint32_t duration_us)
    {
        // 3. Trigger pulse
        EXPECT_CALL(gpio_hal, set_level(TRIG_PIN, 1)).WillOnce(Return(ESP_OK));
        EXPECT_CALL(sys_rom_hal, delay_us(duration_us)).WillOnce(Return());
        EXPECT_CALL(gpio_hal, set_level(TRIG_PIN, 0)).WillOnce(Return(ESP_OK));
    }

    void ExpectRisingEdge(
        int64_t start_time_us,
        uint32_t loops_until_high = 1, // How many loops until HIGH
        bool timeout = false)
    {
        // Initial timestamp
        EXPECT_CALL(timer_hal, get_time_us()).WillOnce(Return(start_time_us));

        if (timeout) {
            // Simulates timeout: gpio always returns LOW
            EXPECT_CALL(gpio_hal, get_level(ECHO_PIN)).WillRepeatedly(Return(0));

            // Timer advances beyond timeout
            EXPECT_CALL(timer_hal, get_time_us()).WillRepeatedly(Return(start_time_us + 50000)); // > timeout_us
            return;
        }
        // 2. Simulates N loops with echo LOW
        for (uint32_t i = 0; i < loops_until_high - 1; i++) {
            EXPECT_CALL(gpio_hal, get_level(ECHO_PIN)).WillOnce(Return(0));

            // Timer check inside the loop (not timeout yet)
            EXPECT_CALL(timer_hal, get_time_us()).WillOnce(Return(start_time_us + (i + 1) * 10));
        }
        // 3. First HIGH (rising edge)
        EXPECT_CALL(gpio_hal, get_level(ECHO_PIN)).WillOnce(Return(1));
        EXPECT_CALL(timer_hal, get_time_us()).WillOnce(Return(start_time_us + loops_until_high * 10));
    }

    void ExpectEchoMeasurement(
        int64_t echo_start_us,
        uint32_t pulse_duration_us,    // How long stays HIGH
        uint32_t loops_while_high = 3, // How many loops while HIGH
        bool timeout = false)
    {
        // 1. Timestamp do início da medição
        EXPECT_CALL(timer_hal, get_time_us()).WillOnce(Return(echo_start_us));

        if (timeout) {
            // Simulates timeout: gpio always returns HIGH, never falls
            EXPECT_CALL(gpio_hal, get_level(ECHO_PIN)).WillRepeatedly(Return(1));

            EXPECT_CALL(timer_hal, get_time_us()).WillRepeatedly(Return(echo_start_us + 50000)); // > timeout
            return;
        }

        // 2. Simulates N loops with echo HIGH
        for (uint32_t i = 0; i < loops_while_high; i++) {
            EXPECT_CALL(gpio_hal, get_level(ECHO_PIN)).WillOnce(Return(1));

            // Timer check (not timeout yet)
            EXPECT_CALL(timer_hal, get_time_us()).WillOnce(Return(echo_start_us + (i + 1) * 100));
        }

        // 3. Last loop: echo goes to LOW (falling edge!)
        EXPECT_CALL(gpio_hal, get_level(ECHO_PIN)).WillOnce(Return(0));

        // 4. Timer check final (not timeout)
        EXPECT_CALL(timer_hal, get_time_us()).WillOnce(Return(echo_start_us + loops_while_high * 100));

        // 5. Timestamp final to calculate duration
        EXPECT_CALL(timer_hal, get_time_us()).WillOnce(Return(echo_start_us + pulse_duration_us));
    }

    void PrepareTrigger()
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
    }

    idf_hals::MockGpioHAL gpio_hal;
    idf_hals::MockTimerHAL timer_hal;
    idf_hals::MockSysRomHAL sys_rom_hal;
    std::unique_ptr<UsDriver> driver;
    UsConfig default_cfg_;

    const gpio_num_t TRIG_PIN = GPIO_NUM_4;
    const gpio_num_t ECHO_PIN = GPIO_NUM_5;
};

TEST_F(UsDriverTest, InitSuccess)
{
    EXPECT_CALL(gpio_hal, reset_pin(_)).Times(2).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(gpio_hal, config(_)).Times(2).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(gpio_hal, set_level(_, 0)).Times(2).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(gpio_hal, set_direction(ECHO_PIN, GPIO_MODE_OUTPUT)).WillOnce(Return(ESP_OK));

    EXPECT_EQ(ESP_OK, driver->init());
}

TEST_F(UsDriverTest, InitFailure)
{
    EXPECT_CALL(gpio_hal, reset_pin(_)).WillOnce(Return(ESP_ERR_INVALID_ARG));
    EXPECT_EQ(ESP_ERR_INVALID_ARG, driver->init());

    EXPECT_CALL(gpio_hal, reset_pin(_)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(gpio_hal, config(_)).WillOnce(Return(ESP_ERR_INVALID_ARG));
    EXPECT_EQ(ESP_ERR_INVALID_ARG, driver->init());

    EXPECT_CALL(gpio_hal, reset_pin(_)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(gpio_hal, config(_)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(gpio_hal, set_level(TRIG_PIN, 0)).WillOnce(Return(ESP_ERR_INVALID_ARG));
    EXPECT_EQ(ESP_ERR_INVALID_ARG, driver->init());

    EXPECT_CALL(gpio_hal, reset_pin(_)).Times(2).WillOnce(Return(ESP_OK)).WillOnce(Return(ESP_ERR_INVALID_ARG));
    EXPECT_CALL(gpio_hal, config(_)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(gpio_hal, set_level(TRIG_PIN, 0)).WillOnce(Return(ESP_OK));
    EXPECT_EQ(ESP_ERR_INVALID_ARG, driver->init());

    EXPECT_CALL(gpio_hal, reset_pin(_)).Times(2).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(gpio_hal, config(_)).WillOnce(Return(ESP_OK)).WillOnce(Return(ESP_ERR_INVALID_ARG));
    EXPECT_CALL(gpio_hal, set_level(TRIG_PIN, 0)).WillOnce(Return(ESP_OK));
    EXPECT_EQ(ESP_ERR_INVALID_ARG, driver->init());

    EXPECT_CALL(gpio_hal, reset_pin(_)).Times(2).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(gpio_hal, config(_)).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(gpio_hal, set_level(_, 0)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(gpio_hal, set_direction(ECHO_PIN, GPIO_MODE_OUTPUT)).WillOnce(Return(ESP_ERR_INVALID_ARG));
    EXPECT_EQ(ESP_ERR_INVALID_ARG, driver->init());

    EXPECT_CALL(gpio_hal, reset_pin(_)).Times(2).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(gpio_hal, config(_)).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(gpio_hal, set_level(_, 0)).WillOnce(Return(ESP_OK)).WillOnce(Return(ESP_ERR_INVALID_ARG));
    EXPECT_CALL(gpio_hal, set_direction(ECHO_PIN, _)).WillOnce(Return(ESP_OK));
    EXPECT_EQ(ESP_ERR_INVALID_ARG, driver->init());
}

TEST_F(UsDriverTest, DeinitSuccess)
{
    EXPECT_CALL(gpio_hal, set_level(_, 0)).Times(2).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(gpio_hal, reset_pin(_)).Times(2).WillRepeatedly(Return(ESP_OK));
    EXPECT_EQ(ESP_OK, driver->deinit());
}

TEST_F(UsDriverTest, DeinitFailure)
{
    EXPECT_CALL(gpio_hal, set_level(_, 0)).WillOnce(Return(ESP_ERR_INVALID_ARG));
    EXPECT_EQ(ESP_ERR_INVALID_ARG, driver->deinit());

    EXPECT_CALL(gpio_hal, set_level(_, 0)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(gpio_hal, reset_pin(_)).WillOnce(Return(ESP_ERR_INVALID_ARG));
    EXPECT_EQ(ESP_ERR_INVALID_ARG, driver->deinit());

    EXPECT_CALL(gpio_hal, set_level(_, 0)).Times(2).WillOnce(Return(ESP_OK)).WillOnce(Return(ESP_ERR_INVALID_ARG));
    EXPECT_CALL(gpio_hal, reset_pin(_)).WillOnce(Return(ESP_OK));
    EXPECT_EQ(ESP_ERR_INVALID_ARG, driver->deinit());

    EXPECT_CALL(gpio_hal, set_level(_, 0)).Times(2).WillRepeatedly(Return(ESP_OK));
    EXPECT_CALL(gpio_hal, reset_pin(_)).Times(2).WillOnce(Return(ESP_OK)).WillOnce(Return(ESP_ERR_INVALID_ARG));
    EXPECT_EQ(ESP_ERR_INVALID_ARG, driver->deinit());
}

TEST_F(UsDriverTest, PingSuccess)
{
    UsConfig cfg;
    cfg.ping_duration_us = 20;
    cfg.timeout_us = 30000;

    int64_t start_time_us = 1000;
    uint32_t echo_duration_us = 1000;

    InSequence s;

    // 1. Prepare
    EXPECT_CALL(gpio_hal, set_direction(ECHO_PIN, GPIO_MODE_OUTPUT)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(gpio_hal, set_level(ECHO_PIN, 0)).WillOnce(Return(ESP_OK));

    // 2. Switch echo
    EXPECT_CALL(gpio_hal, set_direction(ECHO_PIN, GPIO_MODE_INPUT)).WillOnce(Return(ESP_OK));

    // 3. Initial stuck check
    EXPECT_CALL(gpio_hal, get_level(ECHO_PIN)).WillOnce(Return(0));

    // 4. Trigger pulse
    EXPECT_CALL(gpio_hal, set_level(TRIG_PIN, 1)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(sys_rom_hal, delay_us(cfg.ping_duration_us)).WillOnce(Return());
    EXPECT_CALL(gpio_hal, set_level(TRIG_PIN, 0)).WillOnce(Return(ESP_OK));

    // 5. Wait for rising edge
    EXPECT_CALL(timer_hal, get_time_us()).WillOnce(Return(start_time_us)); // start mark
    // Inside loop:
    EXPECT_CALL(gpio_hal, get_level(ECHO_PIN)).WillOnce(Return(1));            // rising edge found
    EXPECT_CALL(timer_hal, get_time_us()).WillOnce(Return(start_time_us + 1)); // timeout check

    // 6. Measure high pulse
    int64_t echo_start = start_time_us + 10;
    EXPECT_CALL(timer_hal, get_time_us()).WillOnce(Return(echo_start)); // echo_start mark
    // Inside loop:
    EXPECT_CALL(gpio_hal, get_level(ECHO_PIN)).WillOnce(Return(0));          // falling edge found
    EXPECT_CALL(timer_hal, get_time_us()).WillOnce(Return(echo_start + 1)); // timeout check

    // 7. Echo ends
    EXPECT_CALL(timer_hal, get_time_us()).WillOnce(Return(echo_start + echo_duration_us)); // echo_end

    auto result = driver->ping_once(cfg);
    EXPECT_EQ(result.result, UsResult::OK);
}

TEST_F(UsDriverTest, RisingEdge_Simple)
{
    InSequence s;

    ExpectSuccessfulPing();

    auto result = driver->ping_once(default_cfg_);
    ASSERT_EQ(result, (Reading{UsResult::OK, 17.15f}));
}

TEST_F(UsDriverTest, RisingEdge_ImmediateHigh)
{
    InSequence s;

    ExpectPingPrepare();
    ExpectStuckCheck(false);
    ExpectTriggerPulse(20);
    ExpectRisingEdge(1000, 1); // 1 loop and already HIGH
    ExpectEchoMeasurement(1010, 1000);

    auto result = driver->ping_once(default_cfg_);
    ASSERT_EQ(result, (Reading{UsResult::OK, 17.15f}));
}

TEST_F(UsDriverTest, RisingEdge_SlowResponse)
{
    InSequence s;

    ExpectPingPrepare();
    ExpectStuckCheck(false);
    ExpectTriggerPulse(20);
    ExpectRisingEdge(1000, 5); // 5 loops until HIGH
    ExpectEchoMeasurement(1050, 1000);

    auto result = driver->ping_once(default_cfg_);
    ASSERT_EQ(result, (Reading{UsResult::OK, 17.15f}));
}

TEST_F(UsDriverTest, EchoMeasurement_NormalDistance)
{
    InSequence s;

    ExpectPingPrepare();
    ExpectStuckCheck(false);
    ExpectTriggerPulse(20);
    ExpectRisingEdge(1000);
    ExpectDistanceMeasurement(17.15f);

    auto result = driver->ping_once(default_cfg_);
    EXPECT_EQ(result.result, UsResult::OK);
    EXPECT_NEAR(result.cm, 17.15f, 0.5f);
}

TEST_F(UsDriverTest, EchoMeasurement_MaxDistance)
{
    InSequence s;

    default_cfg_.max_distance_cm = 610;

    ExpectPingPrepare();
    ExpectStuckCheck(false);
    ExpectTriggerPulse(20);
    ExpectRisingEdge(1000);

    ExpectDistanceMeasurement(600.0f);

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

    auto result = driver->ping_once(default_cfg_);
    EXPECT_EQ(result.result, UsResult::OK);
    EXPECT_NEAR(result.cm, 10.1f, 0.5f);

    ExpectPingPrepare();
    ExpectStuckCheck(false);
    ExpectTriggerPulse(20);
    ExpectRisingEdge(1000);

    ExpectDistanceMeasurement(100.0f);

    result = driver->ping_once(default_cfg_);
    EXPECT_EQ(result.result, UsResult::OK);
    EXPECT_NEAR(result.cm, 100.0f, 0.5f);
}

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

    EXPECT_CALL(gpio_hal, set_direction(ECHO_PIN, GPIO_MODE_OUTPUT)).WillOnce(Return(ESP_FAIL));

    auto result = driver->ping_once(default_cfg_);
    EXPECT_EQ(UsResult::HW_FAULT, result.result);

    EXPECT_CALL(gpio_hal, set_direction(ECHO_PIN, GPIO_MODE_OUTPUT)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(gpio_hal, set_level(ECHO_PIN, 0)).WillOnce(Return(ESP_FAIL));

    result = driver->ping_once(default_cfg_);
    EXPECT_EQ(UsResult::HW_FAULT, result.result);

    EXPECT_CALL(gpio_hal, set_direction(ECHO_PIN, GPIO_MODE_OUTPUT)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(gpio_hal, set_level(ECHO_PIN, 0)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(gpio_hal, set_direction(ECHO_PIN, GPIO_MODE_INPUT)).WillOnce(Return(ESP_FAIL));

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

    PrepareTrigger();
    EXPECT_CALL(gpio_hal, set_level(TRIG_PIN, 1)).WillOnce(Return(ESP_FAIL));

    auto result = driver->ping_once(default_cfg_);
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
}

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
