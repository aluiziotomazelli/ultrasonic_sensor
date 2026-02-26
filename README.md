# Ultrasonic Sensor Component

[![ESP-IDF Build](https://github.com/aluiziotomazelli/ultrasonic_sensor/actions/workflows/build.yml/badge.svg)](https://github.com/aluiziotomazelli/ultrasonic_sensor/actions/workflows/build.yml)
[![Host tests](https://github.com/aluiziotomazelli/ultrasonic_sensor/actions/workflows/host_test.yml/badge.svg)](https://github.com/aluiziotomazelli/ultrasonic_sensor/actions/workflows/host_test.yml)

[![Coverage Report](https://img.shields.io/badge/coverage-report-blue)](https://aluiziotomazelli.github.io/ultrasonic_sensor/coverage/index.html)


A robust, test-driven ESP-IDF component for HC-SR04 and similar ultrasonic sensors. Designed for high reliability in real-world conditions, featuring advanced error handling and a clean, decoupled architecture.

## Overview

This component provides a high-level C++ interface to measure distances using ultrasonic sensors. It goes beyond simple pulse timing by incorporating statistical filtering and specific hardware fault detections learned from bench testing.

#### Supported Sensors
- RCWL--1655 and JSN-SR04T (waterproof, single transducer)
- HC-SR04 (dual transducer)
- Any HC-SR04-compatible ultrasonic sensor

## Features

- **Hardware Abstraction Layer**: Clean separation between hardware and logic layers
- **Statistical Filtering**: Median and dominant cluster algorithms to handle noise and outliers
- **Comprehensive Error Handling**: Distinguishes hardware failures from logical errors for proper recovery
- **Extensively Tested**: Unit and integration tests with high coverage running on Linux host
- **Well Documented**: Complete API reference and usage examples

## Architecture

The project follows solid design principles to ensure maintainability and testability:
- **Dependency Injection**: All hardware dependencies (GPIO, Timers) are injected via interfaces.
- **Interface-Based Design**: Core logic is decoupled from hardware implementation, allowing for easy mocking.
- **Separation of Concerns**:
  - `UsSensor`: The public orchestrator that provides the final distance.
    - Manages ping loop
    - Coordinates driver and processor
    
  - `UsDriver`: Handles low-level pulse triggering and echo timing.
    - GPIO protocol (trigger/echo)
    - Timing measurements
    - Hardware error detection

  - `UsProcessor`: Responsible for statistical analysis and error refinement.
    - Filters valid samples
    - Calculates variance
    - Applies median/cluster algorithms
    - Error detection and refinement
  - HAL: Hardware Abstraction Layer
    - `IGpioHAL`, `ITimerHAL` interfaces
    - Enables testing without hardware


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

### [Tests Report](https://aluiziotomazelli.github.io/ultrasonic_sensor/) 

[![Coverage Report](https://img.shields.io/badge/coverage-report-blue)](https://aluiziotomazelli.github.io/ultrasonic_sensor/coverage/index.html)


The component includes comprehensive unit and integration tests that run on Linux host (no hardware required). The tests cover all edge cases and error states. Reliability is ensured through:
- **Host-based Testing**: Unit tests run on Linux using Google Test and Google Mock.
- **High Coverage**: Comprehensive test suite covering edge cases and error states.
- **CI Pipeline**: Automated builds and tests for every push.

The tests are located in the [host_test](host_test) directory, with a [README](host_test/README.md) file that explains how to run them.

## API Reference

Detailed documentation for all classes and methods can be found in [API.md](API.md).

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

## Author

[aluiziotomazelli](https://github.com/aluiziotomazelli)
