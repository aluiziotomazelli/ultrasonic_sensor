// components/ultrasonic_sensor/src/us_driver.cpp

#include "us_driver.hpp"

#include <cstdint>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

namespace ultrasonic {

static const char *TAG = "UsDriver";

UsDriver::UsDriver(
    idf_hals::IGpioHAL &gpio_hal,
    idf_hals::ITimerHAL &timer_hal,
    idf_hals::ISysRomHAL &sys_rom_hal,
    gpio_num_t trig_pin,
    gpio_num_t echo_pin)
    : gpio_hal_(gpio_hal)
    , timer_hal_(timer_hal)
    , sys_rom_hal_(sys_rom_hal)
    , trig_pin_(trig_pin)
    , echo_pin_(echo_pin)
{
}

esp_err_t UsDriver::init()
{
    ESP_LOGD(TAG, "Initializing UsDriver: TRIG=%d, ECHO=%d", trig_pin_, echo_pin_);

    esp_err_t ret;

    // Configure TRIG pin as output
    ret = gpio_hal_.reset_pin(trig_pin_);
    if (ret != ESP_OK)
        return ret;

    gpio_config_t trig_conf = {
        .pin_bit_mask = 1ULL << trig_pin_,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_hal_.config(&trig_conf);
    if (ret != ESP_OK)
        return ret;

    ret = gpio_hal_.set_level(trig_pin_, 0);
    if (ret != ESP_OK)
        return ret;

    // Configure ECHO pin as input
    ret = gpio_hal_.reset_pin(echo_pin_);
    if (ret != ESP_OK)
        return ret;

    gpio_config_t echo_conf = {
        .pin_bit_mask = 1ULL << echo_pin_,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_hal_.config(&echo_conf);
    if (ret != ESP_OK)
        return ret;

    // Pulse echo low to clear any residual state
    ret = gpio_hal_.set_direction(echo_pin_, GPIO_MODE_OUTPUT);
    if (ret != ESP_OK)
        return ret;

    ret = gpio_hal_.set_level(echo_pin_, 0);
    if (ret != ESP_OK)
        return ret;

    return ESP_OK;
}

esp_err_t UsDriver::deinit()
{
    // Reset pins to a safe state
    esp_err_t ret;

    ret = gpio_hal_.set_level(trig_pin_, 0);
    if (ret != ESP_OK)
        return ret;
    ret = gpio_hal_.reset_pin(trig_pin_);
    if (ret != ESP_OK)
        return ret;

    ret = gpio_hal_.set_level(echo_pin_, 0);
    if (ret != ESP_OK)
        return ret;
    ret = gpio_hal_.reset_pin(echo_pin_);
    if (ret != ESP_OK)
        return ret;

    return ESP_OK;
}

Reading UsDriver::ping_once(const UsConfig &cfg)
{
    // 1. Prepare: set ECHO as output low to clear residual state
    if (gpio_hal_.set_direction(echo_pin_, GPIO_MODE_OUTPUT) != ESP_OK)
        return {UsResult::HW_FAULT, 0.0f};
    if (gpio_hal_.set_level(echo_pin_, 0) != ESP_OK)
        return {UsResult::HW_FAULT, 0.0f};

    // 2. Switch ECHO to input for measurement
    if (gpio_hal_.set_direction(echo_pin_, GPIO_MODE_INPUT) != ESP_OK)
        return {UsResult::HW_FAULT, 0.0f};

    // 3. Check if ECHO is stuck HIGH before triggering
    if (is_echo_stuck())
        return {UsResult::ECHO_STUCK, 0.0f};

    // 4. Send trigger pulse
    esp_err_t ret = trigger(cfg.ping_duration_us);
    if (ret != ESP_OK)
        return {UsResult::HW_FAULT, 0.0f};

    // 5. Wait for rising edge (start of echo pulse)
    ret = wait_rising_edge(cfg.timeout_us);
    if (ret == ESP_ERR_TIMEOUT)
        return {UsResult::TIMEOUT, 0.0f};
    if (ret != ESP_OK)
        return {UsResult::HW_FAULT, 0.0f};

    // 6. Measure the HIGH pulse duration
    uint32_t duration_us = 0;
    ret = measure_pulse(cfg.timeout_us, duration_us);
    if (ret == ESP_ERR_TIMEOUT)
        return {UsResult::TIMEOUT, 0.0f};
    if (ret != ESP_OK)
        return {UsResult::HW_FAULT, 0.0f};

    // 7. Convert to distance
    float cm = (duration_us * SOUND_SPEED_CM_PER_US) / 2.0f;

    if (cm < cfg.min_distance_cm || cm > cfg.max_distance_cm) {
        ESP_LOGD(TAG, "Out of range: %.1f cm (limits: %.1f-%.1f)", cm, cfg.min_distance_cm, cfg.max_distance_cm);
        return {UsResult::OUT_OF_RANGE, 0.0f};
    }

    return {UsResult::OK, cm};
}

bool UsDriver::is_echo_stuck()
{
    return gpio_hal_.get_level(echo_pin_) != 0;
}

esp_err_t UsDriver::trigger(uint16_t pulse_duration_us)
{
    esp_err_t ret;

    ret = gpio_hal_.set_level(trig_pin_, 1);
    if (ret != ESP_OK)
        return ret;

    sys_rom_hal_.delay_us(pulse_duration_us);

    ret = gpio_hal_.set_level(trig_pin_, 0);
    return ret;
}

esp_err_t UsDriver::wait_rising_edge(uint32_t timeout_us)
{
    int64_t start = timer_hal_.get_time_us();
    int level = 0;

    do {
        level = gpio_hal_.get_level(echo_pin_);
        int64_t now = timer_hal_.get_time_us();

        if (level != 0) {
            return ESP_OK;
        }

        if (now - start > timeout_us)
            return ESP_ERR_TIMEOUT;
    } while (true);
}

esp_err_t UsDriver::measure_pulse(uint32_t timeout_us, uint32_t &duration_us)
{
    int64_t echo_start = timer_hal_.get_time_us();
    int level = 1;

    do {
        level = gpio_hal_.get_level(echo_pin_);
        int64_t now = timer_hal_.get_time_us();

        if (level == 0) {
            break;
        }

        if (now - echo_start > timeout_us)
            return ESP_ERR_TIMEOUT;
    } while (true);

    int64_t echo_end = timer_hal_.get_time_us();
    duration_us = static_cast<uint32_t>(echo_end - echo_start);
    return ESP_OK;
}

} // namespace ultrasonic
