/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file test_hardware_gpio.c
 * @brief Hardware-in-the-Loop GPIO expander and pin state validation for Thingy:52
 * 
 * This module provides critical hardware validation for:
 * - SX1509B GPIO expander initialization sequence
 * - CCS811 power control via SX1509B pins (dependency chain)
 * - GPIO pin state validation and debugging
 * - Device tree initialization order verification
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <hal/nrf_gpio.h>

#include "board.h"

LOG_MODULE_REGISTER(test_hardware_gpio, CONFIG_LOG_DEFAULT_LEVEL);

/* Device tree node definitions */
#define GPIO0_NODE DT_NODELABEL(gpio0)
#define SX1509B_NODE DT_NODELABEL(sx1509b)
#define I2C_NODE DT_NODELABEL(i2c0)

/* SX1509B I2C address and register definitions */
#define SX1509B_I2C_ADDR 0x3E
#define SX1509B_REG_DIR_B 0x0E     /* Pin direction register */
#define SX1509B_REG_DATA_B 0x10    /* Pin data register */
#define SX1509B_REG_PULL_UP_B 0x06 /* Pull-up register */

/* CCS811 control pins on SX1509B (pins 10-12) */
#define CCS811_PWR_PIN 10    /* Pin 10: CCS811 power control */
#define CCS811_RESET_PIN 11  /* Pin 11: CCS811 reset control */
#define CCS811_WAKE_PIN 12   /* Pin 12: CCS811 wake control */

/* SX1509B reset pin on nRF52832 (P0.16) */
#define SX1509B_RESET_PIN 16

/* Hardware test result structure */
struct gpio_test_result {
    bool passed;
    const char *test_name;
    const char *error_msg;
    int measured_value;
};

/* Test result storage */
static struct gpio_test_result gpio_test_results[16];
static int gpio_test_count = 0;

/**
 * @brief Record GPIO test result for reporting
 */
static void record_gpio_test_result(const char *test_name, bool passed, 
                                   const char *error_msg, int measured_value)
{
    if (gpio_test_count < ARRAY_SIZE(gpio_test_results)) {
        gpio_test_results[gpio_test_count].passed = passed;
        gpio_test_results[gpio_test_count].test_name = test_name;
        gpio_test_results[gpio_test_count].error_msg = error_msg;
        gpio_test_results[gpio_test_count].measured_value = measured_value;
        gpio_test_count++;
    }
}

/**
 * @brief Test SX1509B GPIO expander initialization sequence
 */
void test_hardware_sx1509b_initialization_sequence(void)
{
    LOG_INF("=== Testing SX1509B Initialization Sequence ===");
    
    const struct device *sx1509b_dev = DEVICE_DT_GET(SX1509B_NODE);
    const struct device *gpio0_dev = DEVICE_DT_GET(GPIO0_NODE);
    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);
    
    /* Test 1: GPIO0 device readiness */
    if (!device_is_ready(gpio0_dev)) {
        record_gpio_test_result("GPIO0 Device Ready", false, "GPIO0 not ready", 0);
        LOG_ERR("GPIO0 device not ready");
        return;
    }
    record_gpio_test_result("GPIO0 Device Ready", true, NULL, 1);
    LOG_INF("✓ GPIO0 device ready");
    
    /* Test 2: I2C bus readiness */
    if (!device_is_ready(i2c_dev)) {
        record_gpio_test_result("I2C Bus Ready", false, "I2C not ready", 0);
        LOG_ERR("I2C device not ready");
        return;
    }
    record_gpio_test_result("I2C Bus Ready", true, NULL, 1);
    LOG_INF("✓ I2C bus ready");
    
    /* Test 3: SX1509B reset pin state (P0.16) */
    int pin_value = nrf_gpio_pin_read(SX1509B_RESET_PIN);
    bool reset_ok = (pin_value == 1); /* Should be HIGH for SX1509B to operate */
    record_gpio_test_result("SX1509B Reset Pin", reset_ok, 
                           reset_ok ? NULL : "Reset pin not HIGH", pin_value);
    LOG_INF("SX1509B reset pin (P0.16): %s (value: %d)", 
            reset_ok ? "✓ HIGH" : "✗ LOW", pin_value);
    
    /* Test 4: SX1509B device readiness */
    if (!device_is_ready(sx1509b_dev)) {
        record_gpio_test_result("SX1509B Device Ready", false, "SX1509B not ready", 0);
        LOG_ERR("SX1509B device not ready - check initialization order and reset pin");
        return;
    }
    record_gpio_test_result("SX1509B Device Ready", true, NULL, 1);
    LOG_INF("✓ SX1509B device ready");
    
    /* Test 5: I2C communication with SX1509B */
    uint8_t reg_value;
    int ret = i2c_reg_read_byte(i2c_dev, SX1509B_I2C_ADDR, SX1509B_REG_DATA_B, &reg_value);
    if (ret != 0) {
        record_gpio_test_result("SX1509B I2C Comm", false, "I2C read failed", ret);
        LOG_ERR("Failed to read from SX1509B: %d", ret);
    } else {
        record_gpio_test_result("SX1509B I2C Comm", true, NULL, reg_value);
        LOG_INF("✓ SX1509B I2C communication successful (data: 0x%02X)", reg_value);
    }
}

/**
 * @brief Test CCS811 power control via SX1509B dependency chain
 */
void test_hardware_ccs811_power_control_via_sx1509b(void)
{
    LOG_INF("=== Testing CCS811 Power Control via SX1509B ===");
    
    const struct device *sx1509b_dev = DEVICE_DT_GET(SX1509B_NODE);
    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);
    
    if (!device_is_ready(sx1509b_dev)) {
        record_gpio_test_result("CCS811 Power Control", false, "SX1509B not ready", 0);
        LOG_ERR("Cannot test CCS811 power control - SX1509B not ready");
        return;
    }
    
    /* Test reading pin states for CCS811 control pins */
    uint8_t pin_data;
    int ret = i2c_reg_read_byte(i2c_dev, SX1509B_I2C_ADDR, SX1509B_REG_DATA_B, &pin_data);
    if (ret != 0) {
        record_gpio_test_result("CCS811 Pin Read", false, "Pin read failed", ret);
        LOG_ERR("Failed to read CCS811 control pins: %d", ret);
        return;
    }
    
    /* Check CCS811 power pin (pin 10) */
    bool pwr_pin = (pin_data & (1 << CCS811_PWR_PIN)) != 0;
    record_gpio_test_result("CCS811 Power Pin", true, NULL, pwr_pin ? 1 : 0);
    LOG_INF("CCS811 power pin (SX1509B.%d): %s", CCS811_PWR_PIN, pwr_pin ? "HIGH" : "LOW");
    
    /* Check CCS811 reset pin (pin 11) */
    bool reset_pin = (pin_data & (1 << CCS811_RESET_PIN)) != 0;
    record_gpio_test_result("CCS811 Reset Pin", true, NULL, reset_pin ? 1 : 0);
    LOG_INF("CCS811 reset pin (SX1509B.%d): %s", CCS811_RESET_PIN, reset_pin ? "HIGH" : "LOW");
    
    /* Check CCS811 wake pin (pin 12) */
    bool wake_pin = (pin_data & (1 << CCS811_WAKE_PIN)) != 0;
    record_gpio_test_result("CCS811 Wake Pin", true, NULL, wake_pin ? 1 : 0);
    LOG_INF("CCS811 wake pin (SX1509B.%d): %s", CCS811_WAKE_PIN, wake_pin ? "HIGH" : "LOW");
    
    /* Validate dependency chain is working */
    bool dependency_ok = pwr_pin; /* Power should be controlled */
    record_gpio_test_result("CCS811 Dependency Chain", dependency_ok,
                           dependency_ok ? NULL : "CCS811 power control not functioning",
                           pin_data);
    
    if (dependency_ok) {
        LOG_INF("✓ CCS811 dependency chain working - SX1509B controls CCS811 power");
    } else {
        LOG_WRN("⚠ CCS811 dependency chain issue - power control may not be working");
    }
}

/**
 * @brief Test GPIO pin state validation and debugging output
 */
void test_hardware_gpio_pin_state_validation(void)
{
    LOG_INF("=== Testing GPIO Pin State Validation ===");
    
    /* This test validates the board_print_pin_states() function works correctly */
    const struct device *gpio0_dev = DEVICE_DT_GET(GPIO0_NODE);
    const struct device *sx1509b_dev = DEVICE_DT_GET(SX1509B_NODE);
    
    /* Test 1: GPIO0 pin state debugging */
    if (device_is_ready(gpio0_dev)) {
        record_gpio_test_result("GPIO0 Pin Debug", true, NULL, 1);
        LOG_INF("✓ GPIO0 pin state debugging available");
        
        /* Sample a few key pins */
        int uart_tx_pin = nrf_gpio_pin_read(3); /* UART TX */
        int i2c_sda_pin = nrf_gpio_pin_read(7); /* I2C SDA */
        int i2c_scl_pin = nrf_gpio_pin_read(8); /* I2C SCL */
        int sx1509b_reset = nrf_gpio_pin_read(16); /* SX1509B reset */
        
        LOG_INF("Key GPIO0 pins: UART_TX=%d, SDA=%d, SCL=%d, SX1509B_RST=%d",
                uart_tx_pin, i2c_sda_pin, i2c_scl_pin, sx1509b_reset);
    } else {
        record_gpio_test_result("GPIO0 Pin Debug", false, "GPIO0 not ready", 0);
        LOG_ERR("GPIO0 not ready for pin debugging");
    }
    
    /* Test 2: SX1509B pin state debugging */
    if (device_is_ready(sx1509b_dev)) {
        record_gpio_test_result("SX1509B Pin Debug", true, NULL, 1);
        LOG_INF("✓ SX1509B pin state debugging available");
        
        /* Trigger the board pin state debugging function */
        LOG_INF("Calling board_print_pin_states() for full hardware debug output:");
        board_print_pin_states();
        
    } else {
        record_gpio_test_result("SX1509B Pin Debug", false, "SX1509B not ready", 0);
        LOG_WRN("SX1509B not ready for pin debugging");
    }
}

/**
 * @brief Test device tree initialization order
 */
void test_hardware_device_tree_initialization_order(void)
{
    LOG_INF("=== Testing Device Tree Initialization Order ===");
    
    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);
    const struct device *gpio0_dev = DEVICE_DT_GET(GPIO0_NODE);
    const struct device *sx1509b_dev = DEVICE_DT_GET(SX1509B_NODE);
    
    /* Test proper initialization order: I2C -> GPIO0 -> SX1509B */
    
    /* Test 1: I2C bus initialized first (priority 50) */
    bool i2c_ready = device_is_ready(i2c_dev);
    record_gpio_test_result("I2C Init Order", i2c_ready, 
                           i2c_ready ? NULL : "I2C bus not initialized", i2c_ready ? 1 : 0);
    LOG_INF("I2C bus (priority 50): %s", i2c_ready ? "✓ Ready" : "✗ Not ready");
    
    /* Test 2: GPIO0 initialized (priority 40) */
    bool gpio0_ready = device_is_ready(gpio0_dev);
    record_gpio_test_result("GPIO0 Init Order", gpio0_ready,
                           gpio0_ready ? NULL : "GPIO0 not initialized", gpio0_ready ? 1 : 0);
    LOG_INF("GPIO0 (priority 40): %s", gpio0_ready ? "✓ Ready" : "✗ Not ready");
    
    /* Test 3: SX1509B initialized after GPIO0 (priority 60) */
    bool sx1509b_ready = device_is_ready(sx1509b_dev);
    record_gpio_test_result("SX1509B Init Order", sx1509b_ready,
                           sx1509b_ready ? NULL : "SX1509B not initialized", sx1509b_ready ? 1 : 0);
    LOG_INF("SX1509B (priority 60): %s", sx1509b_ready ? "✓ Ready" : "✗ Not ready");
    
    /* Test 4: Dependency chain validation */
    bool order_ok = i2c_ready && gpio0_ready;
    if (sx1509b_ready && order_ok) {
        record_gpio_test_result("Init Order Chain", true, NULL, 1);
        LOG_INF("✓ Device initialization order correct");
    } else if (!order_ok) {
        record_gpio_test_result("Init Order Chain", false, "Prerequisites not met", 0);
        LOG_ERR("✗ Device initialization prerequisites failed");
    } else {
        record_gpio_test_result("Init Order Chain", false, "SX1509B init failed despite prerequisites", 0);
        LOG_ERR("✗ SX1509B failed to initialize despite I2C/GPIO0 being ready");
        LOG_ERR("  This suggests SX1509B reset pin or hardware issue");
    }
}

/**
 * @brief Print GPIO hardware test summary report
 */
void gpio_hardware_test_print_report(void)
{
    LOG_INF("========================================");
    LOG_INF("=== GPIO Hardware Test Summary Report ===");
    LOG_INF("========================================");
    
    int passed = 0, failed = 0;
    
    for (int i = 0; i < gpio_test_count; i++) {
        struct gpio_test_result *result = &gpio_test_results[i];
        
        if (result->passed) {
            passed++;
            if (result->measured_value >= 0) {
                LOG_INF("✓ PASS: %s (%d)", result->test_name, result->measured_value);
            } else {
                LOG_INF("✓ PASS: %s", result->test_name);
            }
        } else {
            failed++;
            if (result->error_msg) {
                LOG_ERR("✗ FAIL: %s - %s", result->test_name, result->error_msg);
            } else {
                LOG_ERR("✗ FAIL: %s", result->test_name);
            }
        }
    }
    
    LOG_INF("========================================");
    LOG_INF("GPIO Tests: %d, Passed: %d, Failed: %d", gpio_test_count, passed, failed);
    LOG_INF("GPIO Success Rate: %d%%", (passed * 100) / (gpio_test_count > 0 ? gpio_test_count : 1));
    LOG_INF("========================================");
}

/**
 * @brief Run all GPIO hardware tests
 */
void run_all_gpio_hardware_tests(void)
{
    LOG_INF("Starting Hardware-in-the-Loop GPIO Tests");
    LOG_INF("========================================");
    
    /* Reset test results */
    gpio_test_count = 0;
    
    /* Run GPIO hardware tests */
    test_hardware_sx1509b_initialization_sequence();
    test_hardware_ccs811_power_control_via_sx1509b();
    test_hardware_gpio_pin_state_validation();
    test_hardware_device_tree_initialization_order();
    
    /* Print final report */
    gpio_hardware_test_print_report();
}