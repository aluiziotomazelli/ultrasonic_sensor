# Ultrasonic Sensor Component API Reference

This document provides a comprehensive reference for the public API of the `ultrasonic_sensor` component.

## Public Interface

### IUsSensor

The primary interface for the ultrasonic sensor orchestrator.

#### `esp_err_t init()`
Initializes the ultrasonic sensor component. Configures the necessary GPIOs and prepares the sensor for measurements.
* **Returns:**
    * `ESP_OK`: Success.
    * `Other`: Error codes propagated from the underlying driver HAL implementation.

#### `esp_err_t deinit()`
Deinitializes the ultrasonic sensor component. Releases resources and resets GPIO pins to a safe state.
* **Returns:**
    * `ESP_OK`: Success.
    * `Other`: Error codes propagated from the underlying driver HAL implementation.

#### `Reading read_distance(uint8_t ping_count)`
Performs a distance measurement using multiple pings and applies statistical filtering.
* **Parameters:**
    * `ping_count`: Number of pings to attempt (limited by `IUsProcessor::MAX_PINGS`, usually 15).
* **Returns:**
    * A `Reading` structure containing the unified `UsResult` and the filtered distance.

---

## Configuration Structures

### UsConfig

Used to configure hardware timing and statistical processing behavior.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `ping_interval_ms` | `uint16_t` | `70` | Delay between consecutive pings (ms). |
| `ping_duration_us` | `uint16_t` | `20` | Duration of the trigger pulse (us). |
| `timeout_us` | `uint32_t` | `30000` | Maximum wait time for an echo pulse (us). |
| `filter` | `Filter` | `MEDIAN` | Statistical filter type to apply. |
| `min_distance_cm` | `float` | `10.0f` | Minimum valid distance threshold (cm). |
| `max_distance_cm` | `float` | `200.0f` | Maximum valid distance threshold (cm). |
| `max_dev_cm` | `float` | `15.0f` | Max standard deviation allowed for an `OK` result (cm). |
| `warmup_time_ms` | `uint16_t` | `600` | Wait time after initialization before first measurement (ms). |

---

## Enumerations

### UsResult

Unified result type for all sensor operations.

| Value | Description |
|-------|-------------|
| `OK` | Reliable reading: high ping ratio and low variance. |
| `WEAK_SIGNAL` | Valid reading, but with lower ping ratio or slightly elevated variance. |
| `TIMEOUT` | Sensor did not respond to trigger within `timeout_us`. |
| `OUT_OF_RANGE` | Measured distance is outside the `[min_distance_cm, max_distance_cm]` range. |
| `HIGH_VARIANCE` | Standard deviation of valid pings exceeds `max_dev_cm`. |
| `INSUFFICIENT_SAMPLES` | Too few valid pings to produce a reliable result. |
| `ECHO_STUCK` | Hardware Error: ECHO pin is stuck HIGH. Suggests a power cycle. |
| `HW_FAULT` | Hardware Error: GPIO or HAL operation failed. |

### Filter

Types of statistical filters available.

| Value | Description |
|-------|-------------|
| `MEDIAN` | Selects the middle value from a sorted series of samples. |
| `DOMINANT_CLUSTER` | Finds and averages the largest cluster of similar values. |

---

## Structures

### Reading

The result of a measurement attempt.

| Field | Type | Description |
|-------|------|-------------|
| `result` | `UsResult` | The status of the measurement. |
| `cm` | `float` | The measured distance in centimeters. Only valid if `is_success(result)` is true. |

---

## Helper Functions

### `bool is_success(UsResult r)`
Utility to check if a measurement resulted in a valid distance.
* **Returns:** `true` if the result is `OK` or `WEAK_SIGNAL`.
