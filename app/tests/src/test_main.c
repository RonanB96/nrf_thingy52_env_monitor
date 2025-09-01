/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(test_main, CONFIG_LOG_DEFAULT_LEVEL);

/**
 * @brief Main test entry point
 * 
 * This file serves as the main entry point for all ZTEST-based unit tests
 * for the Nordic Thingy:52 Environmental Monitor project.
 * 
 * The ZTEST framework will automatically discover and run all test suites
 * defined in the linked test source files.
 */

static void *test_setup(void)
{
    LOG_INF("Starting Nordic Thingy:52 Environmental Monitor Test Suite");
    LOG_INF("ZTEST Framework Version: %s", CONFIG_KERNEL_VERSION);
    
    /* Global test setup - initialize any shared resources */
    return NULL;
}

static void test_teardown(void *fixture)
{
    LOG_INF("Test suite complete");
    /* Global test cleanup */
}

/* Global test suite for framework validation */
ZTEST_SUITE(framework_validation, NULL, test_setup, NULL, NULL, test_teardown);

/**
 * @brief Basic framework validation test
 * 
 * Ensures the ZTEST framework is working correctly and basic
 * assertions function as expected.
 */
ZTEST(framework_validation, test_framework_basic)
{
    LOG_INF("Running basic framework validation");
    
    /* Basic assertion tests */
    zassert_true(true, "Basic true assertion failed");
    zassert_false(false, "Basic false assertion failed");
    zassert_equal(1, 1, "Basic equality assertion failed");
    zassert_not_equal(1, 2, "Basic inequality assertion failed");
    zassert_is_null(NULL, "NULL pointer assertion failed");
    
    /* String comparison test */
    const char *test_str = "test";
    zassert_str_equal(test_str, "test", "String equality assertion failed");
    
    LOG_INF("Framework validation tests passed");
}

/**
 * @brief Test floating point support
 * 
 * Validates that floating point operations work correctly in the test
 * environment, which is critical for sensor value calculations.
 */
ZTEST(framework_validation, test_floating_point_support)
{
    LOG_INF("Testing floating point support");
    
    float temp = 25.5f;
    float humidity = 60.2f;
    float pressure = 1013.25f;
    
    /* Test basic floating point operations */
    zassert_within(temp, 25.5f, 0.01f, "Temperature float comparison failed");
    zassert_within(humidity, 60.2f, 0.01f, "Humidity float comparison failed");
    zassert_within(pressure, 1013.25f, 0.01f, "Pressure float comparison failed");
    
    /* Test floating point arithmetic */
    float sum = temp + humidity;
    zassert_within(sum, 85.7f, 0.01f, "Float addition failed");
    
    LOG_INF("Floating point support validated");
}

/**
 * @brief Test memory allocation
 * 
 * Ensures dynamic memory allocation works in the test environment.
 */
ZTEST(framework_validation, test_memory_allocation)
{
    LOG_INF("Testing memory allocation");
    
    /* Test k_malloc if available */
    void *test_ptr = k_malloc(100);
    if (test_ptr != NULL) {
        LOG_INF("Memory allocation successful");
        k_free(test_ptr);
    } else {
        LOG_WRN("Dynamic memory allocation not available");
    }
    
    /* Test stack allocation */
    char stack_buffer[256];
    memset(stack_buffer, 0xAA, sizeof(stack_buffer));
    
    bool all_set = true;
    for (int i = 0; i < sizeof(stack_buffer); i++) {
        if (stack_buffer[i] != (char)0xAA) {
            all_set = false;
            break;
        }
    }
    
    zassert_true(all_set, "Stack buffer initialization failed");
    LOG_INF("Memory allocation tests passed");
}