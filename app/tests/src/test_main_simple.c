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
 * @file test_main_simple.c
 * @brief Simple test file to verify build and coverage setup
 *
 * This file contains basic tests to validate the test framework
 * and coverage reporting setup works correctly.
 */

/**
 * @brief Test suite for framework validation - simple version
 */
ZTEST_SUITE(framework_validation, NULL, NULL, NULL, NULL, NULL);

/**
 * @brief Basic framework test
 */
ZTEST(framework_validation, test_basic_functionality)
{
    LOG_INF("TEST: Basic framework functionality");
    
    /* Basic assertions */
    zassert_true(true, "Basic true assertion must pass");
    zassert_false(false, "Basic false assertion must pass");
    zassert_equal(1, 1, "Integer equality must pass");
    zassert_not_equal(1, 2, "Integer inequality must pass");
    
    /* String comparison */
    zassert_str_equal("test", "test", "String equality must pass");
    
    LOG_INF("PASS: Basic framework test completed");
}

/**
 * @brief Floating point test
 */
ZTEST(framework_validation, test_floating_point)
{
    LOG_INF("TEST: Floating point operations");
    
    float temp = 25.5f;
    float tolerance = 0.01f;
    
    zassert_within(temp, 25.5f, tolerance, "Float comparison must work");
    
    /* Test arithmetic */
    float sum = temp + 10.0f;
    zassert_within(sum, 35.5f, tolerance, "Float arithmetic must work");
    
    LOG_INF("PASS: Floating point test completed");
}

/**
 * @brief Memory operations test
 */
ZTEST(framework_validation, test_memory_operations)
{
    LOG_INF("TEST: Memory operations");
    
    /* Stack buffer test */
    uint8_t buffer[64];
    memset(buffer, 0xAA, sizeof(buffer));
    
    /* Verify pattern */
    for (size_t i = 0; i < sizeof(buffer); i++) {
        zassert_equal(buffer[i], 0xAA, "Buffer pattern must be correct");
    }
    
    LOG_INF("PASS: Memory operations test completed");
}

/**
 * @brief Build system validation
 */
ZTEST_SUITE(build_validation, NULL, NULL, NULL, NULL, NULL);

/**
 * @brief Test build configuration
 */
ZTEST(build_validation, test_build_config)
{
    LOG_INF("TEST: Build configuration");
    
    /* Verify ZTEST is enabled */
    zassert_true(IS_ENABLED(CONFIG_ZTEST), "ZTEST must be enabled");
    
    #ifdef CONFIG_COVERAGE
    LOG_INF("Coverage reporting enabled");
    #endif
    
    LOG_INF("PASS: Build configuration test completed");
}

/**
 * @brief Test system functionality
 */
ZTEST(build_validation, test_system_functions)
{
    LOG_INF("TEST: System functionality");
    
    /* Basic arithmetic */
    int a = 10, b = 20;
    int result = a + b;
    zassert_equal(result, 30, "Arithmetic must work");
    
    /* String operations */
    int len = strlen("test");
    zassert_equal(len, 4, "String functions must work");
    
    LOG_INF("PASS: System functionality test completed");
}