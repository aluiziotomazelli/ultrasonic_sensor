# Ultrasonic Sensor Component

[![ESP-IDF Build](https://github.com/aluiziotomazelli/ultrasonic_sensor/actions/workflows/build.yml/badge.svg)](https://github.com/aluiziotomazelli/ultrasonic_sensor/actions/workflows/build.yml)
[![Host tests](https://github.com/aluiziotomazelli/ultrasonic_sensor/actions/workflows/build.yml/badge.svg)](https://github.com/aluiziotomazelli/ultrasonic_sensor/actions/workflows/build.yml)

A robust, test-driven ESP-IDF component for HC-SR04 and similar ultrasonic sensors. Designed for high reliability in real-world conditions, featuring advanced error handling and a clean, decoupled architecture.

## Overview

This component provides a high-level C++ interface to measure distances using ultrasonic sensors. It goes beyond simple pulse timing by incorporating statistical filtering and specific hardware fault detections learned from bench testing.

## Architecture

The project follows solid design principles to ensure maintainability and testability:
- **Dependency Injection**: All hardware dependencies (GPIO, Timers) are injected via interfaces.
- **Interface-Based Design**: Core logic is decoupled from hardware implementation, allowing for easy mocking.
- **Separation of Concerns**:
  - `UsDriver`: Handles low-level pulse triggering and echo timing.
  - `UsProcessor`: Responsible for statistical analysis and error refinement.
  - `UsSensor`: The public orchestrator that provides the final distance.

## Usage

```cpp
#include "us_sensor.hpp"

// Initialize with Echo and Trigger pins
UsConfig cfg = {}; // Use default settings
UsSensor sensor(GPIO_NUM_4, GPIO_NUM_5, cfg);

sensor.init();

// Read distance in centimeters (uses internal filtering)
float distance = sensor.read_distance();

if (distance > 0) {
    printf("Distance: %.2f cm\n", distance);
}
```
*For complete implementation details, check the [examples/](examples/) folder.*

## Hardware Lessons & Error Handling

Bench testing with real hardware revealed several edge cases that this component specifically addresses:

- **ECHO_STUCK**: In long-running applications or high-EMI environments, the Echo pin can occasionally remain HIGH indefinitely.
  - *Recovery*: If this error persists, the recommended solution is to power cycle the sensor. This requires high-side switching (using a PNP transistor or a GPIO that supports the required current). **Avoid low-side NPN switching**, as it can lead to `HIGH_VARIANCE` issues.
- **HIGH_VARIANCE**: Occurs when the sensor GND connection is weak or disconnected. The sensor may still return values due to "ghost paths" from VCC through the Echo pin, but they will be random and unstable. This error helps detect wiring failures.
- **HW_FAULT**: Indicates internal driver failures (e.g., ESP-IDF GPIO functions returning errors).
- **TIMEOUT**: Sensor did not respond to the trigger pulse within the configured `timeout_us`.
- **OUT_OF_RANGE**: Sensor responded, but the object is outside the physically reliable measurement range.

## Testing

Reliability is ensured through:
- **Host-based Testing**: Unit tests run on Linux using Google Test and Google Mock.
- **High Coverage**: Comprehensive test suite covering edge cases and error states.
- **CI Pipeline**: Automated builds and tests for every push.

## API Reference

Detailed documentation for all classes and methods can be found in [API.md](API.md).

## License

This project is licensed under the MIT License.

## Author

[aluiziotomazelli](https://github.com/aluiziotomazelli)
