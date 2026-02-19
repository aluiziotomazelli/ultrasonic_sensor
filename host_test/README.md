# Host Testing with Google Test (GTest)

This directory contains host-based tests for the component. Host testing allows you to run unit tests on your development machine (Linux) instead of the target microcontroller, enabling faster development cycles and the use of advanced mocking frameworks like Google Mock.

## Google Test Integration

Instead of bundling the Google Test source code in this repository, we use CMake's **FetchContent** module. This approach keeps the repository lightweight and ensures we can easily manage dependencies.

### The GTest Wrapper Component

In ESP-IDF, every piece of code is treated as a component. To integrate GTest seamlessly, we use a "wrapper component" located in `host_test/gtest`.

This component's `CMakeLists.txt` is responsible for:
1. **Fetching GTest**: Downloading the specified version of Google Test/Mock from GitHub.
2. **ESP-IDF Integration**: Registering itself as an IDF component so that other components (like your tests) can simply use `REQUIRES gtest`.
3. **Handling the Build Flow**: Using guards like `CMAKE_BUILD_EARLY_EXPANSION` to ensure the download only happens during the actual build phase, preventing errors during IDF's component requirement discovery.

## How to Run Tests

### Prerequisites

- ESP-IDF environment set up and sourced.
- A Linux host machine (or WSL).
- The project root directory must be named `ultrasonic_sensor`. 
  - Idf build system uses "Preset Component Variables" that are available for use, but should not be modified: 
  `COMPONENT_NAME`: Name of the component. Same as the name of the component directory.

### Execution Steps

1. Navigate to the test project directory:
   ```bash
   cd host_test/test_us_sensor
   ```

2. Set the target to Linux:
   ```bash
   idf.py --preview set-target linux
   ```

3. Build the test:
   ```bash
   idf.py build
   ```

4. Run the executable:
   ```bash
   ./build/test_us_sensor.elf
   ```

## Code Coverage

You can generate code coverage reports (in `.info` format) using `lcov`.

### Generating Coverage

1. Build the test project (coverage is enabled by default for Linux):
   ```bash
   idf.py build
   ```

2. Run the coverage target:
   ```bash
   # from the root test directory
   ninja -C build generate_coverage

   # or from the build directory
   cd build
   ninja generate_coverage
   ```
   *Note: You can also run it via idf.py: `idf.py build generate_coverage`.*

This will:
- Clean up old `.gcda` files and previous reports.
- Run the test suite.
- Capture coverage data and filter it to include only the component source files (src and include), excluding tests and external libraries.
- Generate an HTML report in the `coverage/` directory at the test directory root.
- Print a summary to the console.

## Unified Testing and Coverage (All Tests)

For production readiness or CI/CD, you can run all tests and generate a single unified coverage report.

### Running All Tests with CTest

1. Ensure all tests are built:
   ```bash
   cd host_test
   mkdir -p build && cd build
   cmake ..
   cmake --build . --target build_all_tests
   ```

2. Run all tests:
   ```bash
   ctest
   ```

### Unified Coverage Report

After running the tests (either individually or via `ctest`), you can generate a consolidated report:

1. Generate the unified report:
   ```bash
   # From the host_test/build directory
   cmake --build . --target unified_coverage
   ```

This will search for coverage data across all test build directories and generate a combined report in `host_test/coverage/`.

## Shared Coverage Logic

The coverage logic is centralized in `host_test/coverage_common.cmake`. Individual test projects include this file to maintain consistency and reduce duplication.

## Why FetchContent?

- **Clean Repo**: No need to store thousands of lines of external code.
- **Version Control**: Easy to upgrade GTest by changing a single URL/Tag.
- **Automation**: The library is automatically downloaded and built as part of the standard `idf.py build` flow.
