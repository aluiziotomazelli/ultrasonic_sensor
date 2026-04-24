# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.1] - 2026-04-24

### Changed
- Moved all classes and types to the `ultrasonic` namespace.
- Inlined `GpioHAL` and `TimerHAL` implementations into their headers for better performance.

### Removed
- `src/us_gpio_hal.cpp` and `src/us_timer_hal.cpp` (now inlined).

---

## [1.0.0] - 2026-04-24

### Added
- Initial release of Ultrasonic Sensor component for ESP-IDF
- Robust C++ interface for HC-SR04 and compatible sensors
- Statistical filtering (Median and Dominant Cluster)
- Hardware Abstraction Layer (HAL) for GPIO and Timers
- Comprehensive unit and integration tests

[1.0.1]: https://github.com/aluiziotomazelli/ultrasonic_sensor/releases/tag/v1.0.1
[1.0.0]: https://github.com/aluiziotomazelli/ultrasonic_sensor/releases/tag/v1.0.0