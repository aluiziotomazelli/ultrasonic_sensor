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

    float samples[MAX_PINGS];
    uint8_t count = 0;
    uint8_t timeouts = 0;
    uint8_t out_of_range = 0;

    for (uint8_t i = 0; i < ping_count; i++) {
        Reading raw = driver_->ping_once(cfg_);

        // Hardware failures abort the loop immediately — application must act
        if (raw.result == UsResult::ECHO_STUCK || raw.result == UsResult::HW_FAULT) {
            ESP_LOGE(TAG, "Hardware failure on ping %d: %d — aborting", i, static_cast<int>(raw.result));
            return raw;
        }

        // Logical failures (TIMEOUT, OUT_OF_RANGE) discard the ping and continue
        if (is_success(raw.result)) {
            samples[count++] = raw.cm;
        }
        else {
            ESP_LOGD(TAG, "Ping %d discarded: result=%d", i, static_cast<int>(raw.result));
            if (raw.result == UsResult::TIMEOUT) {
                timeouts++;
            }
            else if (raw.result == UsResult::OUT_OF_RANGE) {
                out_of_range++;
            }
        }

        // Note: inter-ping delay (cfg_.ping_interval_ms) is applied inside UsDriver::ping_once()
    }

    // Delegate statistical processing to the processor
    auto result = processor_->process(samples, count, ping_count, cfg_);

    // If processing failed due to lack of samples, refine the error based on what happened during pings
    if (result.result == UsResult::INSUFFICIENT_SAMPLES) {
        if (out_of_range >= timeouts && out_of_range > 0) {
            return {UsResult::OUT_OF_RANGE, 0.0f};
        }
        if (timeouts > 0) {
            return {UsResult::TIMEOUT, 0.0f};
        }
    }

    return {result.result, result.cm};
}
