/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(test_main, CONFIG_LOG_DEFAULT_LEVEL);

/**
 * @file test_main.c
 * @brief Main test entry point for Nordic Thingy:52 Environmental Monitor
 *
 * This file serves as the main entry point for all ZTEST-based unit tests.
 * It follows C unit testing best practices with proper test organization,
 * clear naming conventions, and comprehensive test coverage.
 *
 * Test Organization:
 * - Framework validation tests ensure ZTEST works correctly
 * - Each test has a clear purpose and descriptive name
 * - Setup/teardown functions provide clean test environments
 * - Comprehensive assertions with meaningful error messages
 */

/**
 * @brief Global test fixture for framework validation
 */
struct framework_test_fixture {
    bool initialized;
    char test_buffer[256];
    int test_counter;
};

/**
 * @brief Test suite setup - runs once before all tests in the suite
 *
 * Following C unit testing best practices:
 * - Initialize shared resources once
 * - Log test suite start
 * - Return fixture data for use in tests
 */
static void *framework_test_suite_setup(void)
{
    LOG_INF("=== Starting Nordic Thingy:52 Environmental Monitor Test Suite ===");
    LOG_INF("ZTEST Framework initialized successfully");
    LOG_INF("Test suite setup: initializing framework validation tests");

    struct framework_test_fixture *fixture = k_malloc(sizeof(struct framework_test_fixture));
    if (!fixture) {
        LOG_ERR("Failed to allocate test fixture");
        return NULL;
    }

    /* Initialize fixture with known state */
    fixture->initialized = true;
    memset(fixture->test_buffer, 0, sizeof(fixture->test_buffer));
    fixture->test_counter = 0;

    LOG_INF("Framework test fixture initialized successfully");
    return fixture;
}

/**
 * @brief Test setup - runs before each individual test
 *
 * Best practices:
 * - Reset test state for each test
 * - Ensure test isolation
 * - Log test start for debugging
 */
static void framework_test_before_each(void *fixture)
{
    struct framework_test_fixture *f = (struct framework_test_fixture *)fixture;
    
    if (!f) {
        return;
    }
    
    /* Reset test state for each test */
    memset(f->test_buffer, 0, sizeof(f->test_buffer));
    f->test_counter = 0;
    
    LOG_DBG("Test setup: framework test state reset");
}

/**
 * @brief Test teardown - runs after each individual test
 *
 * Best practices:
 * - Clean up any test-specific resources
 * - Verify test completed cleanly
 */
static void framework_test_after_each(void *fixture)
{
    struct framework_test_fixture *f = (struct framework_test_fixture *)fixture;
    
    if (!f) {
        return;
    }
    
    /* Verify fixture integrity */
    if (!f->initialized) {
        LOG_WRN("Test fixture not initialized properly");
    }
    
    LOG_DBG("Test teardown: framework test completed cleanly");
}

/**
 * @brief Test suite teardown - runs once after all tests in suite
 *
 * Best practices:
 * - Clean up shared resources
 * - Log test suite completion
 * - Verify overall test state
 */
static void framework_test_suite_teardown(void *fixture)
{
    struct framework_test_fixture *f = (struct framework_test_fixture *)fixture;
    
    LOG_INF("Framework test suite teardown: cleaning up resources");
    
    if (f) {
        if (!f->initialized) {
            LOG_WRN("Fixture not initialized at teardown");
        }
        k_free(f);
    }
    
    LOG_INF("=== Framework validation tests completed successfully ===");
}

/* Define test suite with proper setup/teardown following best practices */
ZTEST_SUITE(framework_validation, NULL, framework_test_suite_setup, 
           framework_test_before_each, framework_test_after_each, 
           framework_test_suite_teardown);

/**
 * @brief Test basic assertions and framework functionality
 *
 * Test Description: Validates that ZTEST framework basic assertions work correctly.
 * This is the foundation for all other tests.
 *
 * Test Coverage:
 * - Boolean assertions (true/false)
 * - Equality assertions (equal/not_equal)
 * - Null pointer assertions
 * - String comparison assertions
 *
 * Expected Results: All assertions should pass, demonstrating framework works.
 */
ZTEST_F(framework_validation, test_basic_assertions)
{
    LOG_INF("TEST: Basic assertions and framework functionality");
    
    /* Test fixture access */
    zassert_not_null(fixture, "Test fixture should be available");
    zassert_true(fixture->initialized, "Test fixture should be initialized");
    
    /* Boolean assertions with descriptive messages */
    zassert_true(true, "Basic true assertion must pass");
    zassert_false(false, "Basic false assertion must pass");
    
    /* Equality assertions */
    zassert_equal(1, 1, "Integer equality (1 == 1) must pass");
    zassert_not_equal(1, 2, "Integer inequality (1 != 2) must pass");
    zassert_equal(0, 0, "Zero equality must pass");
    
    /* Null pointer assertions */
    zassert_is_null(NULL, "NULL pointer assertion must pass");
    zassert_not_null(&fixture->initialized, "Non-null pointer assertion must pass");
    
    /* String comparison with proper error messages */
    const char *expected_string = "test_string";
    const char *actual_string = "test_string";
    zassert_str_equal(expected_string, actual_string, 
                     "String equality assertion must pass for identical strings");
    
    /* Update fixture state to test isolation */
    fixture->test_counter = 1;
    
    LOG_INF("PASS: Basic assertions test completed successfully");
}

/**
 * @brief Test floating point operations and precision
 *
 * Test Description: Validates floating point support in test environment.
 * Critical for environmental sensor value testing.
 *
 * Test Coverage:
 * - Floating point value comparisons with tolerance
 * - Basic arithmetic operations
 * - Precision validation for sensor-like values
 * - Range validation for typical environmental readings
 *
 * Expected Results: All floating point operations work with expected precision.
 */
ZTEST_F(framework_validation, test_floating_point_precision)
{
    LOG_INF("TEST: Floating point operations and precision");
    
    /* Environmental sensor typical values */
    const float temperature_celsius = 25.5f;
    const float humidity_percent = 60.2f;
    const float pressure_hpa = 1013.25f;
    const float co2_ppm = 400.0f;
    
    /* Test precision with environmentally relevant tolerances */
    const float temp_tolerance = 0.1f;      /* ±0.1°C precision */
    const float humidity_tolerance = 0.5f;  /* ±0.5% precision */
    const float pressure_tolerance = 0.01f; /* ±0.01 hPa precision */
    const float co2_tolerance = 1.0f;       /* ±1 ppm precision */
    
    /* Validate sensor value precision */
    zassert_within(temperature_celsius, 25.5f, temp_tolerance, 
                  "Temperature precision must be within ±0.1°C");
    zassert_within(humidity_percent, 60.2f, humidity_tolerance,
                  "Humidity precision must be within ±0.5%");
    zassert_within(pressure_hpa, 1013.25f, pressure_tolerance,
                  "Pressure precision must be within ±0.01 hPa");
    zassert_within(co2_ppm, 400.0f, co2_tolerance,
                  "CO2 precision must be within ±1 ppm");
    
    /* Test arithmetic operations for sensor calculations */
    float temperature_sum = temperature_celsius + 10.0f;
    float expected_sum = 35.5f;
    zassert_within(temperature_sum, expected_sum, temp_tolerance,
                  "Temperature arithmetic (addition) must be accurate");
    
    float temperature_average = (temperature_celsius + 30.5f) / 2.0f;
    float expected_average = 28.0f;
    zassert_within(temperature_average, expected_average, temp_tolerance,
                  "Temperature arithmetic (averaging) must be accurate");
    
    /* Test edge cases for environmental ranges */
    float extreme_cold = -40.0f;  /* Arctic conditions */
    float extreme_hot = 85.0f;    /* Desert conditions */
    
    zassert_true(extreme_cold < temperature_celsius, 
                "Arctic temperature should be less than room temperature");
    zassert_true(extreme_hot > temperature_celsius,
                "Desert temperature should be greater than room temperature");
    
    /* Store test result in fixture for teardown validation */
    strncpy(fixture->test_buffer, "floating_point_test_passed", 
            sizeof(fixture->test_buffer) - 1);
    
    LOG_INF("PASS: Floating point precision test completed successfully");
}

/**
 * @brief Test memory operations and buffer management
 *
 * Test Description: Validates memory allocation, manipulation, and safety.
 * Critical for sensor data buffering and string operations.
 *
 * Test Coverage:
 * - Stack buffer allocation and initialization
 * - Memory pattern testing
 * - Buffer boundary validation
 * - String operations safety
 *
 * Expected Results: All memory operations complete safely without corruption.
 */
ZTEST_F(framework_validation, test_memory_operations)
{
    LOG_INF("TEST: Memory operations and buffer management");
    
    /* Test stack buffer allocation - typical for sensor data */
    uint8_t sensor_data_buffer[64];  /* Typical sensor data size */
    const uint8_t test_pattern = 0xAA;
    const uint8_t alternate_pattern = 0x55;
    
    /* Initialize buffer with known pattern */
    memset(sensor_data_buffer, test_pattern, sizeof(sensor_data_buffer));
    
    /* Verify pattern initialization */
    bool pattern_correct = true;
    for (size_t i = 0; i < sizeof(sensor_data_buffer); i++) {
        if (sensor_data_buffer[i] != test_pattern) {
            pattern_correct = false;
            LOG_ERR("Pattern mismatch at index %zu: expected 0x%02X, got 0x%02X",
                   i, test_pattern, sensor_data_buffer[i]);
            break;
        }
    }
    zassert_true(pattern_correct, "Buffer initialization pattern must be correct");
    
    /* Test partial buffer modification */
    const size_t partial_size = sizeof(sensor_data_buffer) / 2;
    memset(sensor_data_buffer, alternate_pattern, partial_size);
    
    /* Verify partial modification */
    for (size_t i = 0; i < partial_size; i++) {
        zassert_equal(sensor_data_buffer[i], alternate_pattern,
                     "First half should have alternate pattern");
    }
    for (size_t i = partial_size; i < sizeof(sensor_data_buffer); i++) {
        zassert_equal(sensor_data_buffer[i], test_pattern,
                     "Second half should retain original pattern");
    }
    
    /* Test string operations in fixture buffer */
    const char *test_message = "sensor_reading_123";
    size_t message_len = strlen(test_message);
    
    zassert_true(message_len < sizeof(fixture->test_buffer),
                "Test message must fit in fixture buffer");
    
    /* Safe string copy */
    strncpy(fixture->test_buffer, test_message, sizeof(fixture->test_buffer) - 1);
    fixture->test_buffer[sizeof(fixture->test_buffer) - 1] = '\0';  /* Ensure null termination */
    
    /* Verify string operations */
    zassert_str_equal(fixture->test_buffer, test_message,
                     "String copy must preserve content");
    zassert_equal(strlen(fixture->test_buffer), message_len,
                 "String length must be preserved");
    
    /* Update test counter for fixture validation */
    fixture->test_counter = 42;
    
    LOG_INF("PASS: Memory operations test completed successfully");
}

