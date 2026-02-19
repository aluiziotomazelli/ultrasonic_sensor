#include "esp_log.h"

#include "us_sensor.hpp"

extern "C" void app_main(void)
{
    ESP_LOGI("main", "Testing component compilation");

    UsConfig cfg = {};
    UsSensor us(GPIO_NUM_2, GPIO_NUM_4, cfg);

    us.init();
    us.read_distance(7);
    us.deinit();
}
