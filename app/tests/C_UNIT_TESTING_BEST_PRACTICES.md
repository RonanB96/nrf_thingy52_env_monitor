# C Unit Testing Best Practices Guide

## Overview

This document outlines C unit testing best practices implemented in the Nordic Thingy:52 Environmental Monitor test framework, following industry standards and embedded systems requirements.

## Test Organization Principles

### 1. Test Structure and Naming

**Test File Naming Convention:**
- `test_<module_name>.c` - Primary test file for module
- `test_<feature>_<specific_aspect>.c` - Feature-specific tests
- Files should mirror the source structure: `src/sensor_manager.c` → `tests/src/test_sensor_manager.c`

**Test Function Naming:**
```c
// Pattern: test_<module>_<functionality>_<condition>
ZTEST(sensor_manager, test_hts221_read_temperature_success)
ZTEST(sensor_manager, test_hts221_read_temperature_i2c_error)
ZTEST(sensor_manager, test_battery_voltage_low_threshold)
```

**Test Suite Organization:**
- Group related tests in logical suites
- Use descriptive suite names that match modules or features
- Separate integration tests from unit tests

### 2. Test Fixtures and Setup/Teardown

**Best Practice Implementation:**
```c
struct sensor_test_fixture {
    struct device mock_i2c_dev;
    struct device mock_sensor_dev;
    bool initialized;
    int test_iteration;
};

static void *sensor_test_suite_setup(void)
{
    /* One-time setup for entire test suite */
    struct sensor_test_fixture *fixture = k_malloc(sizeof(*fixture));
    
    /* Initialize mocks once */
    mock_i2c_init();
    mock_sensor_init();
    
    fixture->initialized = true;
    return fixture;
}

static void sensor_test_before_each(void *f)
{
    struct sensor_test_fixture *fixture = f;
    
    /* Reset state before each test for isolation */
    mock_i2c_reset();
    mock_sensor_reset();
    fixture->test_iteration++;
}
```

### 3. Assertion Best Practices

**Comprehensive Error Messages:**
```c
/* BAD: Minimal error information */
zassert_equal(result, 0, "failed");

/* GOOD: Descriptive error with context */
zassert_equal(result, 0, 
    "HTS221 temperature read should succeed, got error %d", result);

/* EXCELLENT: Context + expected vs actual + debugging info */
zassert_equal(sensor_value_to_float(&temp), 25.5f,
    "Temperature reading mismatch: expected %.2f°C, got %.2f°C "
    "(I2C status: %d, sensor state: %s)",
    25.5f, sensor_value_to_float(&temp), 
    mock_i2c_get_last_status(), mock_sensor_get_state_string());
```

**Tolerance-Based Comparisons:**
```c
/* Use appropriate tolerances for floating point values */
const float TEMPERATURE_TOLERANCE = 0.1f;  /* ±0.1°C */
const float HUMIDITY_TOLERANCE = 0.5f;     /* ±0.5% */
const float PRESSURE_TOLERANCE = 0.01f;    /* ±0.01 hPa */

zassert_within(measured_temp, expected_temp, TEMPERATURE_TOLERANCE,
    "Temperature precision must be within ±%.1f°C", TEMPERATURE_TOLERANCE);
```

### 4. Mock Framework Standards

**Consistent Mock Interface:**
```c
/* Mock initialization - called once per test suite */
void mock_i2c_init(void);

/* Mock reset - called before each test */
void mock_i2c_reset(void);

/* Mock expectation setup */
void mock_i2c_expect_transaction(const struct mock_i2c_transaction *expected);

/* Mock verification */
void mock_i2c_verify_complete(void);

/* Mock state inspection */
int mock_i2c_get_transaction_count(void);
bool mock_i2c_has_pending_transactions(void);
```

**Transaction-Based Mocking:**
```c
/* Set up expected I2C transaction */
struct mock_i2c_transaction expected = {
    .type = MOCK_I2C_WRITE_READ,
    .addr = HTS221_I2C_ADDR,
    .write_buf = (uint8_t[]){HTS221_WHO_AM_I_REG},
    .write_len = 1,
    .read_buf = (uint8_t[]){HTS221_WHO_AM_I_VALUE},
    .read_len = 1,
    .expected_return = 0
};

mock_i2c_expect_transaction(&expected);

/* Execute actual code under test */
int result = hts221_read_who_am_i(&mock_device);

/* Verify transaction completed */
zassert_ok(result, "WHO_AM_I read should succeed");
mock_i2c_verify_complete();
```

### 5. Test Coverage Best Practices

**Code Coverage Targets:**
- **Unit Tests**: 95%+ line coverage, 85%+ branch coverage
- **Integration Tests**: 80%+ line coverage with hardware simulation
- **Critical Paths**: 100% coverage for safety-critical sensor readings

**Coverage Measurement:**
```bash
# Generate coverage with build
west build app/tests -b native_sim -- -DCONF_FILE=prj_coverage.conf

# Run tests and collect coverage
./build/tests/zephyr/zephyr.exe

# Generate reports
gcovr --root . --filter 'app/src/.*' --html coverage.html
lcov --capture --directory build/tests --output-file coverage.info
```

**Coverage Analysis Guidelines:**
- Exclude test files from coverage: `--exclude 'app/tests/.*'`
- Exclude legacy code: `--exclude 'app/src/main_mesh.c'`
- Focus on active modules: `--filter 'app/src/(sensor_manager|battery_service|ess_service).*'`
- Review uncovered lines for necessary test additions

### 6. Error Condition Testing

**Comprehensive Error Testing:**
```c
ZTEST_F(sensor_manager, test_hts221_i2c_communication_error)
{
    /* Setup: I2C failure scenario */
    mock_i2c_set_next_error(-EIO);
    
    /* Execute: Attempt sensor read */
    int result = sensor_manager_read_temperature();
    
    /* Verify: Proper error handling */
    zassert_equal(result, -EIO, "Should propagate I2C error");
    zassert_false(sensor_manager_has_valid_data(), 
                 "Should not have valid data after I2C error");
    
    /* Verify: System remains stable */
    zassert_true(sensor_manager_is_operational(),
                "System should remain operational after I2C error");
}

ZTEST_F(sensor_manager, test_sensor_power_failure_recovery)
{
    /* Setup: Power failure during sensor operation */
    mock_gpio_set_pin_failure(SX1509B_CCS811_PWR_PIN, true);
    
    /* Execute: Attempt sensor operation */
    int result = sensor_manager_read_air_quality();
    
    /* Verify: Graceful degradation */
    zassert_equal(result, -ENODEV, "Should detect power failure");
    zassert_false(sensor_manager_is_air_quality_valid(),
                 "Air quality should be invalid after power failure");
    
    /* Setup: Power restoration */
    mock_gpio_set_pin_failure(SX1509B_CCS811_PWR_PIN, false);
    
    /* Execute: Recovery attempt */
    result = sensor_manager_recover();
    
    /* Verify: Successful recovery */
    zassert_ok(result, "Should recover after power restoration");
}
```

### 7. Test Documentation Standards

**Test Function Documentation:**
```c
/**
 * @brief Test HTS221 temperature reading under normal conditions
 *
 * Test Description: Validates that the HTS221 sensor can successfully
 * read temperature values under normal operating conditions.
 *
 * Test Setup:
 * - Mock I2C device configured for HTS221 communication
 * - Expected temperature value: 25.5°C
 * - I2C transactions configured to simulate successful reads
 *
 * Test Steps:
 * 1. Configure mock I2C for successful WHO_AM_I read
 * 2. Configure mock I2C for temperature register reads
 * 3. Call sensor_manager_read_temperature()
 * 4. Verify temperature value within expected tolerance
 * 5. Verify all I2C transactions completed
 *
 * Expected Results:
 * - Function returns 0 (success)
 * - Temperature reading is 25.5°C ± 0.1°C
 * - All mock I2C transactions are consumed
 * - Sensor manager reports valid temperature data
 *
 * Coverage: This test covers the normal path through temperature
 * reading including I2C communication and data conversion.
 */
ZTEST_F(sensor_manager, test_hts221_temperature_read_normal_conditions)
{
    /* Test implementation */
}
```

### 8. Continuous Integration Integration

**CI/CD Test Pipeline:**
```yaml
- name: Run Unit Tests with Coverage
  run: |
    # Build with coverage enabled
    west build app/tests -b native_sim -- -DCONF_FILE=prj_coverage.conf
    
    # Run tests
    ./build/tests/zephyr/zephyr.exe
    
    # Generate coverage reports
    gcovr --root . --filter 'app/src/.*' \
          --exclude 'app/tests/.*' \
          --html coverage/coverage.html \
          --json coverage/coverage.json
    
    # Check coverage thresholds
    gcovr --root . --filter 'app/src/.*' \
          --exclude 'app/tests/.*' \
          --fail-under-line 90 \
          --fail-under-branch 80
```

### 9. Test Performance and Timing

**Deterministic Test Timing:**
```c
ZTEST_F(sensor_manager, test_sensor_read_timing)
{
    uint32_t start_time = k_uptime_get_32();
    
    int result = sensor_manager_read_all_sensors();
    
    uint32_t elapsed_time = k_uptime_get_32() - start_time;
    
    /* Verify operation completes within expected time */
    zassert_true(elapsed_time < 100, /* 100ms max */
                "Sensor read should complete within 100ms, took %u ms",
                elapsed_time);
    
    zassert_ok(result, "Sensor read should succeed");
}
```

### 10. Memory and Resource Testing

**Memory Leak Detection:**
```c
ZTEST_F(framework_validation, test_memory_allocation_patterns)
{
    /* Get initial memory state */
    size_t initial_free = k_heap_free_get(&k_malloc_heap);
    
    /* Perform operations that allocate/free memory */
    char *test_buffer = k_malloc(1024);
    zassert_not_null(test_buffer, "Memory allocation should succeed");
    
    /* Use the memory */
    memset(test_buffer, 0xAA, 1024);
    
    /* Free the memory */
    k_free(test_buffer);
    
    /* Verify no memory leak */
    size_t final_free = k_heap_free_get(&k_malloc_heap);
    zassert_equal(initial_free, final_free, 
                 "Memory should be fully freed (leak detection)");
}
```

## Testing Anti-Patterns to Avoid

### 1. Avoid Test Dependencies
```c
/* BAD: Tests depend on each other */
static int global_test_state = 0;

ZTEST(bad_example, test_first) {
    global_test_state = 42;  /* Modifies global state */
    zassert_equal(some_function(), 0, "Should work");
}

ZTEST(bad_example, test_second) {
    /* Depends on test_first running first */
    zassert_equal(global_test_state, 42, "Depends on previous test");
}

/* GOOD: Tests are independent */
ZTEST_F(good_example, test_independent_first) {
    fixture->test_state = 42;  /* Uses fixture */
    zassert_equal(some_function(), 0, "Should work");
}

ZTEST_F(good_example, test_independent_second) {
    fixture->test_state = 100;  /* Independent initialization */
    zassert_equal(other_function(), 0, "Should work independently");
}
```

### 2. Avoid Testing Implementation Details
```c
/* BAD: Testing internal implementation */
ZTEST(bad_example, test_internal_counter) {
    /* This test breaks when implementation changes */
    zassert_equal(get_internal_counter(), 0, "Internal counter should be 0");
}

/* GOOD: Testing observable behavior */
ZTEST(good_example, test_sensor_reading_behavior) {
    /* This test remains valid regardless of internal implementation */
    float temp = sensor_manager_get_temperature();
    zassert_within(temp, 25.0f, 1.0f, "Temperature should be reasonable");
}
```

### 3. Avoid Overly Complex Tests
```c
/* BAD: Complex test doing too much */
ZTEST(bad_example, test_everything) {
    /* This test is hard to debug and maintain */
    setup_sensor1();
    setup_sensor2();
    configure_bluetooth();
    for (int i = 0; i < 100; i++) {
        read_all_sensors();
        process_data();
        send_bluetooth();
    }
    verify_everything();
}

/* GOOD: Simple, focused tests */
ZTEST(good_example, test_sensor_read_single) {
    setup_single_sensor();
    float result = read_temperature();
    zassert_within(result, 25.0f, 0.1f, "Temperature read should work");
}

ZTEST(good_example, test_bluetooth_send_single) {
    setup_bluetooth();
    int result = send_temperature_data(25.0f);
    zassert_ok(result, "Bluetooth send should succeed");
}
```

## Summary

Following these C unit testing best practices ensures:
- **Maintainable tests** that are easy to understand and modify
- **Reliable test results** with proper isolation and deterministic behavior
- **Comprehensive coverage** of both success and error conditions
- **Clear documentation** for future developers
- **Efficient debugging** when tests fail
- **Continuous integration** support with automated coverage reporting

The test framework provides a solid foundation for maintaining code quality while developing the environmental monitoring system for the Nordic Thingy:52.