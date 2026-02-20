#include "us_processor.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

static const char *TAG = "UsProcessor";

// Ratio thresholds for ping validity
static constexpr float VALID_PING_RATIO = 0.7f;   // >= this → OK quality
static constexpr float INVALID_PING_RATIO = 0.4f; // <  this → INSUFFICIENT_SAMPLES

// Fraction of max_dev_cm that triggers WEAK_SIGNAL (instead of OK)
static constexpr float WEAK_VARIANCE_RATIO = 0.6f;

// Dominant cluster parameters
static constexpr float CLUSTER_DELTA_CM = 5.0f; // max spread within a cluster
static constexpr size_t CLUSTER_MIN_SIZE = 2;   // minimum cluster size to be considered

Reading UsProcessor::process(const Reading *pings, uint8_t total_pings, const UsConfig &cfg)
{
    if (total_pings == 0) {
        return {UsResult::INSUFFICIENT_SAMPLES, 0.0f};
    }

    float samples[MAX_PINGS];
    uint8_t valid_count = 0;
    uint8_t timeouts = 0;
    uint8_t out_of_range = 0;

    // 1. Extract valid samples and count specific errors
    for (uint8_t i = 0; i < total_pings; i++) {
        if (is_success(pings[i].result)) {
            samples[valid_count++] = pings[i].cm;
        }
        else if (pings[i].result == UsResult::TIMEOUT) {
            timeouts++;
        }
        else if (pings[i].result == UsResult::OUT_OF_RANGE) {
            out_of_range++;
        }
    }

    // 2. Compute valid ping ratio
    float ratio = static_cast<float>(valid_count) / total_pings;

    // 3. Check minimum data threshold
    if (ratio < INVALID_PING_RATIO) {
        ESP_LOGD(TAG, "Insufficient samples: ratio=%.2f (need >= %.2f)", ratio, INVALID_PING_RATIO);

        // Refine the error based on what happened during pings
        if (out_of_range >= timeouts && out_of_range > 0) {
            return {UsResult::OUT_OF_RANGE, 0.0f};
        }
        if (timeouts > 0) {
            return {UsResult::TIMEOUT, 0.0f};
        }
        return {UsResult::INSUFFICIENT_SAMPLES, 0.0f};
    }

    // 4. Check variance
    float std_dev = get_std_dev(samples, valid_count);
    if (std_dev > cfg.max_dev_cm) {
        ESP_LOGD(TAG, "High variance: std_dev=%.2f cm (limit=%.2f cm)", std_dev, cfg.max_dev_cm);
        return {UsResult::HIGH_VARIANCE, 0.0f};
    }

    // 5. Apply filter
    float distance_cm;
    if (cfg.filter == Filter::MEDIAN) {
        distance_cm = reduce_median(samples, valid_count);
    }
    else {
        distance_cm = reduce_dominant_cluster(samples, valid_count);
    }

    // 6. Determine quality based on ping ratio
    if (ratio >= VALID_PING_RATIO) {
        // Good ratio — check if variance is elevated (but still within limit)
        if (std_dev > cfg.max_dev_cm * WEAK_VARIANCE_RATIO) {
            ESP_LOGD(TAG, "Weak signal (high ratio, elevated variance): std_dev=%.2f", std_dev);
            return {UsResult::WEAK_SIGNAL, distance_cm};
        }
        return {UsResult::OK, distance_cm};
    }

    // Ratio is between INVALID and VALID thresholds → WEAK_SIGNAL
    ESP_LOGD(TAG, "Weak signal (low ratio): ratio=%.2f", ratio);
    return {UsResult::WEAK_SIGNAL, distance_cm};
}

float UsProcessor::reduce_median(float *v, std::size_t n)
{
    std::sort(v, v + n);
    return v[n / 2];
}

float UsProcessor::reduce_dominant_cluster(float *v, std::size_t n)
{
    std::sort(v, v + n);

    float best_cluster_sum = 0.0f;
    size_t best_cluster_size = 0;

    for (size_t i = 0; i < n; i++) {
        float current_sum = 0.0f;
        size_t current_size = 0;

        for (size_t j = i; j < n; j++) {
            if (std::abs(v[j] - v[i]) <= CLUSTER_DELTA_CM) {
                current_sum += v[j];
                current_size++;
            }
            else {
                break; // array is sorted, no need to continue
            }
        }

        if (current_size >= CLUSTER_MIN_SIZE && current_size > best_cluster_size) {
            best_cluster_size = current_size;
            best_cluster_sum = current_sum;
        }
    }

    if (best_cluster_size == 0) {
        ESP_LOGW(TAG, "No valid cluster found, falling back to median");
        return reduce_median(v, n);
    }

    return best_cluster_sum / static_cast<float>(best_cluster_size);
}

float UsProcessor::get_std_dev(const float *samples, uint8_t count)
{
    float mean = 0.0f;
    for (uint8_t i = 0; i < count; i++) mean += samples[i];
    mean /= count;

    float variance = 0.0f;
    for (uint8_t i = 0; i < count; i++) variance += std::pow(samples[i] - mean, 2);
    return std::sqrt(variance / count);
}
