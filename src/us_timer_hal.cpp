#include "us_timer_hal.hpp"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

uint64_t TimerHAL::get_now_us()
{
    return (uint64_t)esp_timer_get_time();
}

esp_err_t TimerHAL::delay_us(uint32_t us)
{
    esp_rom_delay_us(us);
    return ESP_OK;
}

esp_err_t TimerHAL::delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
    return ESP_OK;
}
