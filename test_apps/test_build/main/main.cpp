// components/ultrasonic_sensor/test_apps/test_build/main/main.cpp

#include "esp_log.h"
#include "us_sensor.hpp"
#include "hal_gpio.hpp"
#include "hal_timer.hpp"
#include "hal_sys_rom.hpp"
#include "hal_freertos.hpp"

extern "C" void app_main(void)
{
    using namespace ultrasonic;
    ESP_LOGI("main", "Testing component compilation");

    idf_hals::GpioHAL gpio;
    idf_hals::TimerHAL timer;
    idf_hals::SysRomHAL sys_rom;
    idf_hals::HalFreertos freertos;

    UsConfig cfg = {};
    UsSensor us(gpio, timer, sys_rom, freertos, GPIO_NUM_2, GPIO_NUM_4, cfg);

    us.init();
    us.read_distance(7);
    us.deinit();
}
