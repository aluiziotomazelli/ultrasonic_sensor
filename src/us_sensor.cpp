#include "us_sensor.hpp"
#include "us_driver.hpp"
#include "us_gpio_hal.hpp"
#include "us_processor.hpp"
#include "us_timer_hal.hpp"
#include <memory>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

static const char *TAG = "UsSensor";

// Production constructor: creates concrete HALs and driver internally
UsSensor::UsSensor(gpio_num_t trig_pin, gpio_num_t echo_pin, const UsConfig &cfg)
    : cfg_(cfg)
    , driver_(std::make_shared<UsDriver>(std::make_shared<GpioHAL>(), std::make_shared<TimerHAL>(), trig_pin, echo_pin))
    , processor_(std::make_shared<UsProcessor>())
{
}

// Test constructor: receives dependencies via injection
UsSensor::UsSensor(const UsConfig &cfg, std::shared_ptr<IUsDriver> driver, std::shared_ptr<IUsProcessor> processor)
    : cfg_(cfg)
    , driver_(driver)
    , processor_(processor)
{
}

esp_err_t UsSensor::init()
{
    // Pass warmup_time_ms so the driver waits for sensor stabilization after GPIO setup
    return driver_->init(cfg_.warmup_time_ms);
}

esp_err_t UsSensor::deinit()
{
    return driver_->deinit();
}

Reading UsSensor::read_distance(uint8_t ping_count)
{
    // Clamp ping_count to valid range
    if (ping_count == 0 || ping_count > MAX_PINGS) {
        ESP_LOGW(TAG, "ping_count %d out of range [1, %d], clamping", ping_count, MAX_PINGS);
        ping_count = (ping_count == 0) ? 1 : MAX_PINGS;
    }

    Reading pings[MAX_PINGS];

    for (uint8_t i = 0; i < ping_count; i++) {
        pings[i] = driver_->ping_once(cfg_);

        // Hardware failures abort the loop immediately — application must act
        if (pings[i].result == UsResult::ECHO_STUCK || pings[i].result == UsResult::HW_FAULT) {
            ESP_LOGE(TAG, "Hardware failure on ping %d: %d — aborting", i, static_cast<int>(pings[i].result));
            return pings[i];
        }

        // Logical failures (TIMEOUT, OUT_OF_RANGE) are collected and passed to processor
        if (!is_success(pings[i].result)) {
            ESP_LOGD(TAG, "Ping %d failed: result=%d", i, static_cast<int>(pings[i].result));
        }

        // Note: inter-ping delay (cfg_.ping_interval_ms) is applied inside UsDriver::ping_once()
    }

    // Delegate processing (including logical error refinement) to the processor
    return processor_->process(pings, ping_count, cfg_);
}
