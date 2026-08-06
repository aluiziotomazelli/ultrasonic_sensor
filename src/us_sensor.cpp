// components/ultrasonic_sensor/src/us_sensor.cpp

#include "us_sensor.hpp"

#include <memory>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

#include "us_driver.hpp"
#include "us_processor.hpp"

namespace ultrasonic {

static const char *TAG = "UsSensor";

// Production constructor: creates concrete driver and processor internally
UsSensor::UsSensor(
    idf_hals::IGpioHAL &gpio_hal,
    idf_hals::ITimerHAL &timer_hal,
    idf_hals::ISysRomHAL &sys_rom_hal,
    idf_hals::IHalFreertos &freertos_hal,
    gpio_num_t trig_pin,
    gpio_num_t echo_pin,
    const UsConfig &cfg)
    : cfg_(cfg)
    , driver_(std::make_shared<UsDriver>(gpio_hal, timer_hal, sys_rom_hal, trig_pin, echo_pin))
    , processor_(std::make_shared<UsProcessor>())
    , freertos_hal_(freertos_hal)
{
}

// Test constructor: receives dependencies via injection
UsSensor::UsSensor(
    const UsConfig &cfg,
    std::shared_ptr<IUsDriver> driver,
    std::shared_ptr<IUsProcessor> processor,
    idf_hals::IHalFreertos &freertos_hal)
    : cfg_(cfg)
    , driver_(driver)
    , processor_(processor)
    , freertos_hal_(freertos_hal)
{
}

esp_err_t UsSensor::init()
{
    esp_err_t ret = driver_->init();
    if (ret != ESP_OK) {
        return ret;
    }

    if (cfg_.warmup_time_ms > 0) {
        ESP_LOGD(TAG, "Warming up for %d ms", cfg_.warmup_time_ms);
        freertos_hal_.task_delay(pdMS_TO_TICKS(cfg_.warmup_time_ms));
    }

    return ESP_OK;
}

esp_err_t UsSensor::deinit()
{
    return driver_->deinit();
}

struct PingLogBuffer {
    char data[256];
};

static PingLogBuffer format_pings(const Reading *pings, uint8_t count)
{
    PingLogBuffer buf = {};
    int offset = 0;
    for (uint8_t i = 0; i < count; i++) {
        int written = snprintf(
            buf.data + offset,
            sizeof(buf.data) - offset,
            "%.1f-%d%s",
            pings[i].cm,
            static_cast<int>(pings[i].result),
            (i == count - 1) ? "" : ", ");
        if (written > 0 && static_cast<size_t>(offset + written) < sizeof(buf.data)) {
            offset += written;
        }
    }
    return buf;
}

Reading UsSensor::read_distance(uint8_t ping_count)
{
    // Clamp ping_count to valid range
    if (ping_count == 0 || ping_count > MAX_PINGS) {
        ESP_LOGW(TAG, "ping_count %d out of range [1, %d], clamping", ping_count, MAX_PINGS);
        ping_count = (ping_count == 0) ? 1 : MAX_PINGS;
    }

    Reading pings[MAX_PINGS];
    uint8_t actual_pings = 0;

    for (uint8_t i = 0; i < ping_count; i++) {
        pings[i] = driver_->ping_once(cfg_);
        actual_pings++;

        // Hardware failures abort the loop immediately — application must act
        if (pings[i].result == UsResult::ECHO_STUCK || pings[i].result == UsResult::HW_FAULT) {
            ESP_LOGE(TAG, "Hardware failure on ping %d: %d — aborting", i, static_cast<int>(pings[i].result));
            ESP_LOGD(TAG, "UsSensor: %s (aborted)", format_pings(pings, actual_pings).data);
            return pings[i];
        }

        // Logical failures (TIMEOUT, OUT_OF_RANGE) are collected and passed to processor
        if (!is_success(pings[i].result)) {
            ESP_LOGD(TAG, "Ping %d failed: result=%d", i, static_cast<int>(pings[i].result));
        }

        // Apply inter-ping delay between pings, but not after the last ping
        if (i < ping_count - 1 && cfg_.ping_interval_ms > 0) {
            freertos_hal_.task_delay(pdMS_TO_TICKS(cfg_.ping_interval_ms));
        }
    }

    ESP_LOGD(TAG, "Pings [%u]: %s", actual_pings, format_pings(pings, actual_pings).data);

    // Delegate processing (including logical error refinement) to the processor
    return processor_->process(pings, ping_count, cfg_);
}

} // namespace ultrasonic
