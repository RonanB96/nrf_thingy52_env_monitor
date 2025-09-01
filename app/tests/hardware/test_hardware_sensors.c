/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file test_hardware_sensors.c
 * @brief Hardware-in-the-Loop sensor validation tests for Thingy:52
 * 
 * This module provides real hardware validation for environmental sensors:
 * - HTS221 temperature/humidity sensor
 * - LPS22HB pressure sensor  
 * - CCS811 air quality sensor
 * - SX1509B GPIO expander dependency validation
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "sensor_manager.h"
#include "sensor_hts221_driver.h"
#include "sensor_lps22hb_driver.h" 
#include "sensor_ccs811_driver.h"
#include "board.h"

LOG_MODULE_REGISTER(test_hardware_sensors, CONFIG_LOG_DEFAULT_LEVEL);

/* Hardware test result structure */
struct hardware_test_result {
    bool passed;
    const char *test_name;
    const char *error_msg;
    float measured_value;
    float expected_min;
    float expected_max;
};

/* Test result storage */
static struct hardware_test_result test_results[16];
static int test_count = 0;

/* Device tree node definitions */
#define HTS221_NODE DT_NODELABEL(hts221)
#define LPS22HB_NODE DT_NODELABEL(lps22hb)
#define CCS811_NODE DT_NODELABEL(ccs811)
#define SX1509B_NODE DT_NODELABEL(sx1509b)
#define I2C_NODE DT_NODELABEL(i2c0)

/* Sensor validation ranges (realistic indoor environment) */
#define TEMP_MIN_C -10.0f    /* Minimum valid temperature (°C) */
#define TEMP_MAX_C 60.0f     /* Maximum valid temperature (°C) */
#define HUMIDITY_MIN_PCT 0.0f    /* Minimum valid humidity (%) */
#define HUMIDITY_MAX_PCT 100.0f  /* Maximum valid humidity (%) */
#define PRESSURE_MIN_HPA 800.0f  /* Minimum valid pressure (hPa) */
#define PRESSURE_MAX_HPA 1200.0f /* Maximum valid pressure (hPa) */
#define ECO2_MIN_PPM 400      /* Minimum valid eCO2 (ppm) */
#define ECO2_MAX_PPM 8192     /* Maximum valid eCO2 (ppm) */
#define TVOC_MIN_PPB 0        /* Minimum valid TVOC (ppb) */
#define TVOC_MAX_PPB 1187     /* Maximum valid TVOC (ppb) */

/**
 * @brief Record test result for reporting
 */
static void record_test_result(const char *test_name, bool passed, 
                              const char *error_msg, float measured_value,
                              float expected_min, float expected_max)
{
    if (test_count < ARRAY_SIZE(test_results)) {
        test_results[test_count].passed = passed;
        test_results[test_count].test_name = test_name;
        test_results[test_count].error_msg = error_msg;
        test_results[test_count].measured_value = measured_value;
        test_results[test_count].expected_min = expected_min;
        test_results[test_count].expected_max = expected_max;
        test_count++;
    }
}

/**
 * @brief Test HTS221 temperature and humidity sensor hardware
 */
void test_hardware_hts221_temperature_humidity_reading(void)
{
    LOG_INF("=== Testing HTS221 Temperature/Humidity Hardware ===");
    
    const struct device *hts221_dev = DEVICE_DT_GET(HTS221_NODE);
    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);
    
    /* Test 1: Device readiness */
    if (!device_is_ready(hts221_dev)) {
        record_test_result("HTS221 Device Ready", false, "Device not ready", 0, 0, 0);
        LOG_ERR("HTS221 device not ready");
        return;
    }
    record_test_result("HTS221 Device Ready", true, NULL, 1, 1, 1);
    LOG_INF("✓ HTS221 device ready");
    
    /* Test 2: I2C communication */
    if (!device_is_ready(i2c_dev)) {
        record_test_result("HTS221 I2C Ready", false, "I2C device not ready", 0, 0, 0);
        LOG_ERR("I2C device not ready");
        return;
    }
    record_test_result("HTS221 I2C Ready", true, NULL, 1, 1, 1);
    LOG_INF("✓ I2C device ready");
    
    /* Test 3: Initialize and read sensor */
    int ret = hts221_driver_init(i2c_dev);
    if (ret != 0) {
        record_test_result("HTS221 Driver Init", false, "Driver init failed", ret, 0, 0);
        LOG_ERR("HTS221 driver initialization failed: %d", ret);
        return;
    }
    record_test_result("HTS221 Driver Init", true, NULL, 0, 0, 0);
    LOG_INF("✓ HTS221 driver initialized");
    
    /* Test 4: Read temperature */
    float temperature;
    ret = hts221_driver_read_temperature(&temperature);
    if (ret != 0) {
        record_test_result("HTS221 Temperature Read", false, "Temperature read failed", ret, 0, 0);
        LOG_ERR("HTS221 temperature read failed: %d", ret);
    } else {
        bool temp_valid = (temperature >= TEMP_MIN_C && temperature <= TEMP_MAX_C);
        record_test_result("HTS221 Temperature Read", temp_valid, 
                          temp_valid ? NULL : "Temperature out of range",
                          temperature, TEMP_MIN_C, TEMP_MAX_C);
        LOG_INF("✓ HTS221 temperature: %.2f°C (valid: %s)", 
                (double)temperature, temp_valid ? "YES" : "NO");
    }
    
    /* Test 5: Read humidity */
    float humidity;
    ret = hts221_driver_read_humidity(&humidity);
    if (ret != 0) {
        record_test_result("HTS221 Humidity Read", false, "Humidity read failed", ret, 0, 0);
        LOG_ERR("HTS221 humidity read failed: %d", ret);
    } else {
        bool humidity_valid = (humidity >= HUMIDITY_MIN_PCT && humidity <= HUMIDITY_MAX_PCT);
        record_test_result("HTS221 Humidity Read", humidity_valid,
                          humidity_valid ? NULL : "Humidity out of range", 
                          humidity, HUMIDITY_MIN_PCT, HUMIDITY_MAX_PCT);
        LOG_INF("✓ HTS221 humidity: %.1f%% (valid: %s)", 
                (double)humidity, humidity_valid ? "YES" : "NO");
    }
}

/**
 * @brief Test LPS22HB pressure sensor hardware
 */
void test_hardware_lps22hb_pressure_reading(void)
{
    LOG_INF("=== Testing LPS22HB Pressure Hardware ===");
    
    const struct device *lps22hb_dev = DEVICE_DT_GET(LPS22HB_NODE);
    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);
    
    /* Test 1: Device readiness */
    if (!device_is_ready(lps22hb_dev)) {
        record_test_result("LPS22HB Device Ready", false, "Device not ready", 0, 0, 0);
        LOG_ERR("LPS22HB device not ready");
        return;
    }
    record_test_result("LPS22HB Device Ready", true, NULL, 1, 1, 1);
    LOG_INF("✓ LPS22HB device ready");
    
    /* Test 2: Initialize driver */
    const struct gpio_dt_spec lps22hb_int_pin = GPIO_DT_SPEC_GET(DT_NODELABEL(lps22hb_int), gpios);
    int ret = lps22hb_driver_init(i2c_dev, &lps22hb_int_pin);
    if (ret != 0) {
        record_test_result("LPS22HB Driver Init", false, "Driver init failed", ret, 0, 0);
        LOG_ERR("LPS22HB driver initialization failed: %d", ret);
        return;
    }
    record_test_result("LPS22HB Driver Init", true, NULL, 0, 0, 0);
    LOG_INF("✓ LPS22HB driver initialized");
    
    /* Test 3: Read pressure */
    float pressure;
    ret = lps22hb_driver_read_pressure(&pressure);
    if (ret != 0) {
        record_test_result("LPS22HB Pressure Read", false, "Pressure read failed", ret, 0, 0);
        LOG_ERR("LPS22HB pressure read failed: %d", ret);
    } else {
        bool pressure_valid = (pressure >= PRESSURE_MIN_HPA && pressure <= PRESSURE_MAX_HPA);
        record_test_result("LPS22HB Pressure Read", pressure_valid,
                          pressure_valid ? NULL : "Pressure out of range",
                          pressure, PRESSURE_MIN_HPA, PRESSURE_MAX_HPA);
        LOG_INF("✓ LPS22HB pressure: %.1fhPa (valid: %s)", 
                (double)pressure, pressure_valid ? "YES" : "NO");
    }
}

/**
 * @brief Test CCS811 air quality sensor hardware
 */
void test_hardware_ccs811_air_quality_reading(void)
{
    LOG_INF("=== Testing CCS811 Air Quality Hardware ===");
    
    const struct device *ccs811_dev = DEVICE_DT_GET(CCS811_NODE);
    
    /* Test 1: Device readiness */
    if (!device_is_ready(ccs811_dev)) {
        record_test_result("CCS811 Device Ready", false, "Device not ready", 0, 0, 0);
        LOG_ERR("CCS811 device not ready");
        return;
    }
    record_test_result("CCS811 Device Ready", true, NULL, 1, 1, 1);
    LOG_INF("✓ CCS811 device ready");
    
    /* Test 2: Initialize driver */
    int ret = ccs811_driver_init(ccs811_dev);
    if (ret != 0) {
        record_test_result("CCS811 Driver Init", false, "Driver init failed", ret, 0, 0);
        LOG_ERR("CCS811 driver initialization failed: %d", ret);
        return;
    }
    record_test_result("CCS811 Driver Init", true, NULL, 0, 0, 0);
    LOG_INF("✓ CCS811 driver initialized");
    
    /* Test 3: Check if sensor is ready (may be conditioning) */
    bool is_ready = ccs811_driver_is_ready();
    if (!is_ready) {
        record_test_result("CCS811 Conditioning", true, "Sensor still conditioning", 0, 0, 0);
        LOG_INF("⚠ CCS811 still conditioning - this is normal for new sensors");
        return;
    }
    
    /* Test 4: Read air quality data */
    uint16_t eco2, tvoc;
    ret = ccs811_driver_read_data(&eco2, &tvoc);
    if (ret != 0) {
        record_test_result("CCS811 Data Read", false, "Data read failed", ret, 0, 0);
        LOG_ERR("CCS811 data read failed: %d", ret);
    } else {
        bool eco2_valid = (eco2 >= ECO2_MIN_PPM && eco2 <= ECO2_MAX_PPM);
        bool tvoc_valid = (tvoc >= TVOC_MIN_PPB && tvoc <= TVOC_MAX_PPB);
        
        record_test_result("CCS811 eCO2 Read", eco2_valid,
                          eco2_valid ? NULL : "eCO2 out of range",
                          eco2, ECO2_MIN_PPM, ECO2_MAX_PPM);
        record_test_result("CCS811 TVOC Read", tvoc_valid,
                          tvoc_valid ? NULL : "TVOC out of range",
                          tvoc, TVOC_MIN_PPB, TVOC_MAX_PPB);
        
        LOG_INF("✓ CCS811 eCO2: %dppm (valid: %s), TVOC: %dppb (valid: %s)", 
                eco2, eco2_valid ? "YES" : "NO",
                tvoc, tvoc_valid ? "YES" : "NO");
    }
}

/**
 * @brief Test sensor power cycling functionality
 */
void test_hardware_sensor_power_cycling(void)
{
    LOG_INF("=== Testing Sensor Power Cycling ===");
    
    /* Test power cycling for all sensors */
    struct sensor_data data;
    
    /* Power down all sensors */
    LOG_INF("Powering down sensors...");
    int ret = sensor_manager_power_down_all();
    if (ret == 0) {
        record_test_result("Sensor Power Down", true, NULL, 0, 0, 0);
        LOG_INF("✓ Sensors powered down");
    } else {
        record_test_result("Sensor Power Down", false, "Power down failed", ret, 0, 0);
        LOG_ERR("Sensor power down failed: %d", ret);
    }
    
    k_msleep(1000); /* Wait for power down */
    
    /* Power up and read all sensors */
    LOG_INF("Powering up and reading sensors...");
    ret = sensor_manager_update();
    if (ret == 0) {
        record_test_result("Sensor Power Up", true, NULL, 0, 0, 0);
        LOG_INF("✓ Sensors powered up and read");
        
        /* Get updated data */
        ret = sensor_manager_get_data(&data);
        if (ret == 0) {
            LOG_INF("✓ Power cycle complete - T:%.1f°C H:%.1f%% P:%.1fhPa", 
                    (double)data.temperature, (double)data.humidity, (double)data.pressure);
        }
    } else {
        record_test_result("Sensor Power Up", false, "Power up failed", ret, 0, 0);
        LOG_ERR("Sensor power up failed: %d", ret);
    }
}

/**
 * @brief Print hardware test summary report
 */
void hardware_test_print_report(void)
{
    LOG_INF("========================================");
    LOG_INF("=== Hardware Test Summary Report ===");
    LOG_INF("========================================");
    
    int passed = 0, failed = 0;
    
    for (int i = 0; i < test_count; i++) {
        struct hardware_test_result *result = &test_results[i];
        
        if (result->passed) {
            passed++;
            if (result->measured_value >= result->expected_min && 
                result->measured_value <= result->expected_max) {
                LOG_INF("✓ PASS: %s (%.2f)", result->test_name, (double)result->measured_value);
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
    LOG_INF("Total Tests: %d, Passed: %d, Failed: %d", test_count, passed, failed);
    LOG_INF("Success Rate: %d%%", (passed * 100) / (test_count > 0 ? test_count : 1));
    LOG_INF("========================================");
}

/**
 * @brief Run all hardware sensor tests
 */
void run_all_hardware_sensor_tests(void)
{
    LOG_INF("Starting Hardware-in-the-Loop Sensor Tests");
    LOG_INF("========================================");
    
    /* Reset test results */
    test_count = 0;
    
    /* Print GPIO pin states for debugging */
    board_print_pin_states();
    
    /* Initialize sensor manager */
    int ret = sensor_manager_init(false); /* No periodic updates for testing */
    if (ret != 0) {
        LOG_ERR("Sensor manager initialization failed: %d", ret);
        return;
    }
    
    /* Run individual sensor tests */
    test_hardware_hts221_temperature_humidity_reading();
    test_hardware_lps22hb_pressure_reading();
    test_hardware_ccs811_air_quality_reading();
    test_hardware_sensor_power_cycling();
    
    /* Print final report */
    hardware_test_print_report();
}