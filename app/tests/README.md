# ZTEST Framework Documentation

## Overview

This document describes the Zephyr ZTEST framework implementation for the Nordic Thingy:52 Environmental Monitor project. The test framework enables unit testing of sensor drivers, BLE services, and power management without requiring physical hardware.

## Architecture

### Directory Structure

```
app/tests/
├── CMakeLists.txt              # Test build configuration
├── prj.conf                    # Test-specific Kconfig options
├── testcase.yaml               # Test runner configuration for twister
├── boards/
│   └── native_sim.conf         # Native simulation board config
└── src/
    ├── test_main.c             # Main test runner
    ├── test_mock_framework.c   # Mock framework validation tests
    ├── test_sensor_manager.c   # Sensor manager unit tests
    ├── test_ess_service.c      # ESS service unit tests
    ├── test_battery_service.c  # Battery service unit tests
    └── mocks/
        ├── mock_i2c.{h,c}      # I2C mock implementation
        ├── mock_gpio.{h,c}     # GPIO mock implementation
        ├── mock_sensor.{h,c}   # Sensor API mock implementation
        └── mock_bluetooth.c    # Bluetooth mock implementation
```

### Mock Framework

The test framework includes comprehensive mocks for hardware dependencies:

#### I2C Mock (`mock_i2c.{h,c}`)

- Simulates I2C transactions without hardware
- Supports write, read, and write-read operations
- Transaction validation and error injection
- Used for sensor communication testing

#### GPIO Mock (`mock_gpio.{h,c}`)

- Simulates GPIO pin operations
- Pin configuration, set/get operations
- Interrupt configuration support
- Critical for SX1509B GPIO expander testing

#### Sensor Mock (`mock_sensor.{h,c}`)

- Implements Zephyr sensor API without hardware
- Configurable sensor readings and error conditions
- Sample fetch and channel get simulation
- Call count tracking for verification

## Usage

### Environment Setup

1. **Prerequisites:**
   - Zephyr SDK v0.16+ installed
   - West workspace configured with NCS v3.0.2
   - Python virtual environment with west and dependencies

2. **Build Configuration:**

   ```bash
   # Navigate to project root
   cd nrf_thingy52_env_monitor

   # Build tests for native simulation
   west build app/tests -b native_sim

   # Run tests
   west build -t run
   ```

### Writing Tests

#### Basic Test Structure

```c
#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>
#include "mock_i2c.h"
#include "mock_gpio.h"
#include "mock_sensor.h"

LOG_MODULE_REGISTER(test_example, CONFIG_LOG_DEFAULT_LEVEL);

/* Test setup */
static void *example_setup(void)
{
    mock_i2c_init();
    mock_gpio_init();
    mock_sensor_init();
    return NULL;
}

static void example_before_each(void *fixture)
{
    mock_i2c_reset();
    mock_gpio_reset();
    mock_sensor_reset();
}

ZTEST_SUITE(example_tests, NULL, example_setup, example_before_each, NULL, NULL);

ZTEST(example_tests, test_basic_functionality)
{
    /* Your test code here */
    zassert_true(true, "Basic assertion");
}
```

#### I2C Testing Example

```c
ZTEST(sensor_tests, test_sensor_i2c_communication)
{
    /* Set up expected I2C transaction */
    uint8_t write_data[] = {0x0F};  /* WHO_AM_I register */
    uint8_t expected_response[] = {0xBC};  /* HTS221 ID */
    uint8_t read_buffer[1];

    struct mock_i2c_transaction expected = {
        .type = MOCK_I2C_WRITE_READ,
        .addr = 0x5F,  /* HTS221 I2C address */
        .write_buf = write_data,
        .write_len = 1,
        .read_buf = expected_response,
        .read_len = 1,
        .expected_return = 0
    };

    mock_i2c_expect_transaction(&expected);

    /* Execute the I2C operation */
    struct device mock_i2c_dev = {0};
    int ret = i2c_write_read_mock(&mock_i2c_dev, 0x5F, write_data, 1,
                                 read_buffer, 1);

    /* Verify results */
    zassert_ok(ret, "I2C operation should succeed");
    zassert_equal(read_buffer[0], 0xBC, "Should receive HTS221 ID");

    mock_i2c_verify_complete();
}
```

#### Sensor Testing Example

```c
ZTEST(sensor_tests, test_temperature_reading)
{
    struct device mock_sensor_dev = {0};

    /* Configure expected sensor reading */
    struct sensor_value temp_reading;
    sensor_value_from_float(&temp_reading, 25.5f);

    mock_sensor_set_reading(&mock_sensor_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp_reading);

    /* Test sensor operations */
    int ret = sensor_sample_fetch_mock(&mock_sensor_dev);
    zassert_ok(ret, "Sample fetch should succeed");

    struct sensor_value retrieved_value;
    ret = sensor_channel_get_mock(&mock_sensor_dev, SENSOR_CHAN_AMBIENT_TEMP, &retrieved_value);
    zassert_ok(ret, "Channel get should succeed");

    float retrieved_temp = sensor_value_to_float(&retrieved_value);
    zassert_within(retrieved_temp, 25.5f, 0.01f, "Temperature should match");
}
```

#### GPIO Testing Example

```c
ZTEST(gpio_tests, test_sx1509b_pin_control)
{
    struct device mock_gpio_dev = {0};

    /* Test pin configuration */
    int ret = gpio_pin_configure_mock(&mock_gpio_dev, 10, GPIO_OUTPUT_ACTIVE);
    zassert_ok(ret, "Pin configuration should succeed");

    /* Test pin set */
    ret = gpio_pin_set_mock(&mock_gpio_dev, 10, 1);
    zassert_ok(ret, "Pin set should succeed");

    /* Verify pin state */
    int value = mock_gpio_get_pin_value(&mock_gpio_dev, 10);
    zassert_equal(value, 1, "Pin should be high");

    /* Verify configuration */
    mock_gpio_verify_pin_config(&mock_gpio_dev, 10, GPIO_OUTPUT_ACTIVE);
}
```

### Test Configuration

#### Key Configuration Options (`prj.conf`)

```kconfig
# Enable ZTEST framework
CONFIG_ZTEST=y
CONFIG_ZTEST_MOCKING=y
CONFIG_ZTEST_VERBOSE_OUTPUT=y

# Disable hardware drivers for testing
CONFIG_BT=n
CONFIG_SENSOR=n
CONFIG_I2C=n
CONFIG_GPIO=n

# Enable native simulation
CONFIG_NATIVE_APPLICATION=y

# Logging for debugging
CONFIG_LOG=y
CONFIG_LOG_MODE_IMMEDIATE=y
```

#### Test Categories (`testcase.yaml`)

- **Basic Framework Tests**: Validate ZTEST and mock functionality
- **Sensor Manager Tests**: Test sensor coordination and power management
- **BLE Service Tests**: Test GATT characteristic operations
- **Mock Validation Tests**: Verify mock implementations work correctly

### Running Tests

#### Local Development

```bash
# Build and run all tests
west build app/tests -b native_sim
west build -t run

# Build specific test configuration
west build app/tests -b native_sim -- -DCONF_FILE=prj_verbose.conf

# Run with west test runner (if configured)
west test app/tests
```

#### Continuous Integration

The test framework is designed to integrate with GitHub Actions:

```yaml
# Example CI configuration
- name: Run Unit Tests
  run: |
    west build app/tests -b native_sim
    west build -t run
```

### Debugging Tests

#### Logging

Tests include comprehensive logging for debugging:

```c
LOG_MODULE_REGISTER(test_module, CONFIG_LOG_DEFAULT_LEVEL);

ZTEST(test_suite, test_case)
{
    LOG_INF("Starting test case");
    LOG_DBG("Debug information");

    /* Test code with assertions */

    LOG_INF("Test case completed");
}
```

#### Mock State Inspection

Use mock inspection functions to debug test failures:

```c
/* Check I2C transaction state */
mock_i2c_verify_complete();

/* Check GPIO pin states */
int pin_value = mock_gpio_get_pin_value(&device, pin);

/* Check sensor call counts */
int fetch_count = mock_sensor_get_fetch_call_count(&device);
```

### Best Practices

1. **Reset Mock State**: Always reset mock state between tests
2. **Verify Transactions**: Use `mock_i2c_verify_complete()` to ensure all expected I2C operations occurred
3. **Test Error Conditions**: Test both success and failure scenarios
4. **Use Descriptive Assertions**: Include meaningful error messages in assertions
5. **Log Test Progress**: Use logging to track test execution and debug failures
6. **Test Hardware Interactions**: Focus on testing the interface between software and hardware abstractions

### Limitations

1. **Environment Dependencies**: Requires proper Zephyr SDK installation for native_sim builds
2. **Hardware Abstraction**: Tests validate software logic but not actual hardware behavior
3. **Timing Dependencies**: Mock framework doesn't simulate real-time constraints
4. **Device Tree Integration**: Limited device tree mocking (manual device creation)

### Future Enhancements

1. **Device Tree Mocking**: Automatic mock device generation from device tree
2. **Timing Simulation**: k_msleep and interrupt timing simulation
3. **BLE Stack Mocking**: Complete Bluetooth GATT operation mocking
4. ~~**Coverage Analysis**: Automated code coverage reporting with gcov/lcov~~ ✅ **IMPLEMENTED**
5. **Performance Testing**: Sensor reading timing and power consumption validation

## Code Coverage

The test framework now includes comprehensive code coverage reporting using gcov and lcov tools.

### Coverage Configuration

The framework supports two test configurations:

1. **Standard Testing** (`prj.conf`): Basic test execution
2. **Coverage Testing** (`prj_coverage.conf`): Optimized for coverage data collection

### Generating Coverage Reports

```bash
# Build tests with coverage enabled
west build app/tests -b native_sim -- -DCONF_FILE=prj_coverage.conf

# Run tests to collect coverage data
./build/tests/zephyr/zephyr.exe

# Generate HTML coverage report
lcov --capture --directory build/tests --output-file coverage.info
lcov --remove coverage.info '/usr/*' '/opt/*' '*/tests/*' --output-file coverage_filtered.info
genhtml coverage_filtered.info --output-directory coverage_html

# Generate summary coverage report
gcovr --root . --filter 'app/src/.*' --exclude 'app/tests/.*' --txt
```

### Coverage Metrics

The coverage reporting focuses on:
- **Line Coverage**: Percentage of executable lines tested
- **Branch Coverage**: Percentage of decision branches tested  
- **Function Coverage**: Percentage of functions called during tests

**Target Coverage Levels:**
- Unit Tests: 95%+ line coverage, 85%+ branch coverage
- Integration Tests: 80%+ line coverage
- Critical sensor paths: 100% coverage

### CI/CD Coverage Integration

The GitHub Actions workflow automatically:
- Builds tests with coverage enabled
- Runs all test suites
- Generates HTML and JSON coverage reports
- Uploads coverage artifacts
- Displays coverage summary in test results

Coverage reports are available in the "test-results-and-coverage" artifact after each CI run.

## C Unit Testing Best Practices

The test framework follows industry-standard C unit testing practices. See [C_UNIT_TESTING_BEST_PRACTICES.md](C_UNIT_TESTING_BEST_PRACTICES.md) for comprehensive guidelines covering:

- Test organization and naming conventions
- Setup/teardown patterns for test isolation
- Assertion best practices with descriptive error messages
- Mock framework design patterns
- Error condition testing strategies
- Performance and memory testing approaches
- Coverage analysis and reporting
- CI/CD integration patterns

## Integration with Development Workflow

The test framework integrates with the existing development workflow:

1. **Development**: Write tests alongside feature implementation
2. **Validation**: Run tests locally before committing changes
3. **CI/CD**: Automated test execution in pull requests
4. **Debugging**: Use mock framework to isolate hardware issues
5. **Documentation**: Tests serve as executable documentation of expected behavior

This test framework provides a solid foundation for maintaining code quality and enabling confident refactoring of the environmental monitoring system.
