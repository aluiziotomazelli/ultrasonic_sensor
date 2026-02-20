#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "us_sensor.hpp"

/**
 * Tag for ESP-IDF logs
 */
static const char *TAG = "ULTRASONIC_EXAMPLE";

/**
 * GPIO pin definitions
 */
#define TRIGER_GPIO GPIO_NUM_21
#define ECHO_GPIO GPIO_NUM_7

/**
 * Initial number of pings per measurement
 * Note: The maximum limit defined in the component is MAX_PINGS = 15
 */
#define PINGS_PER_MEASURE 7

extern "C" void app_main(void)
{
    /**
     * Hardware Configurations for RCWL-1655 sensor
     * This is a waterproof sensor and has some specific characteristics
     */
    UsConfig us_cfg = {
        /**
         * 70ms interval between pings to avoid interference from residual echoes
         */
        .ping_interval_ms = 70,

        /**
         * 20us trigger pulse (longer than the standard 10us) to ensure
         * the waterproof sensor transducer is correctly excited
         */
        .ping_duration_us = 20,

        /**
         * 25000us timeout. Considering the speed of sound (~0.0343 cm/us),
         * sound travels ~857cm round trip, allowing measurements up to ~428cm.
         */
        .timeout_us       = 25000,

        /**
         * DOMINANT_CLUSTER filter: groups close measurements and discards outliers,
         * ideal for environments with potential noise or unwanted reflections
         */
        .filter           = Filter::DOMINANT_CLUSTER,

        /**
         * 25cm minimum distance, specific to the RCWL-1655 dead zone
         */
        .min_distance_cm  = 25.0f,

        /**
         * Maximum desired distance for this application
         */
        .max_distance_cm  = 200.0f,

        /**
         * Warmup time. If the sensor is always powered,
         * we can set it to 0 to save time on the first boot
         */
        .warmup_time_ms   = 0
    };

    /**
     * Instantiate the UsSensor component passing the pins and configuration
     */
    UsSensor sensor(TRIGER_GPIO, ECHO_GPIO, us_cfg);

    /**
     * Initialize the sensor (configure GPIOs, etc.)
     */
    ESP_LOGI(TAG, "Initializing ultrasonic sensor...");
    esp_err_t err = sensor.init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize the sensor: %s", esp_err_to_name(err));
        return;
    }

    uint8_t current_pings = PINGS_PER_MEASURE;

    while (1) {
        /**
         * Perform a measurement based on the average/filter of multiple pings
         */
        Reading reading = sensor.read_distance(current_pings);

        /**
         * Check if the reading was successful (OK or WEAK_SIGNAL)
         */
        if (is_success(reading.result)) {
            ESP_LOGI(TAG, "Distance: %.2f cm | Quality: %s (Pings: %u)",
                     reading.cm,
                     (reading.result == UsResult::OK) ? "EXCELLENT" : "WEAK",
                     current_pings);

            /**
             * Dynamic adjustment: if the quality is weak (many lost pings),
             * we increase the number of pings for the next round, respecting the limit of 15.
             */
            if (reading.result == UsResult::WEAK_SIGNAL) {
                if (current_pings < 15) current_pings++;
                ESP_LOGW(TAG, "Weak signal detected. Increasing pings to %u", current_pings);
            } else {
                /**
                 * If the quality is excellent, we return to the default value to save time/energy
                 */
                current_pings = PINGS_PER_MEASURE;
            }
        }
        else {
            /**
             * Detailed error handling
             */
            switch (reading.result) {
                case UsResult::TIMEOUT:
                    ESP_LOGW(TAG, "Error: Sensor did not respond (Timeout). Check connections.");
                    break;
                case UsResult::OUT_OF_RANGE:
                    ESP_LOGW(TAG, "Error: Object out of range (%.1fcm to %.1fcm).",
                             us_cfg.min_distance_cm, us_cfg.max_distance_cm);
                    break;
                case UsResult::HIGH_VARIANCE:
                    ESP_LOGW(TAG, "Error: Too much variance in readings. Object might be moving.");
                    break;
                case UsResult::INSUFFICIENT_SAMPLES:
                    ESP_LOGW(TAG, "Error: Insufficient samples for a reliable calculation.");
                    break;
                case UsResult::ECHO_STUCK:
                    ESP_LOGE(TAG, "CRITICAL ERROR: ECHO pin stuck HIGH! Power-cycle suggested.");
                    break;
                case UsResult::HW_FAULT:
                    ESP_LOGE(TAG, "CRITICAL ERROR: Hardware fault in GPIO driver.");
                    break;
                default:
                    ESP_LOGE(TAG, "Unknown error in reading.");
                    break;
            }
        }

        /**
         * Wait 2000ms before the next measurement
         */
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    /**
     * If you need to deinit(), this is how it should be done
     */
    // err = sensor.deinit();
    // if (err != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to deinitialize: %s", esp_err_to_name(err));
    // }
}
