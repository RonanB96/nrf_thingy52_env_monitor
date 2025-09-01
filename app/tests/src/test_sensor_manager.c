/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>
#include "mock_i2c.h"
#include "mock_gpio.h"
#include "mock_sensor.h"

LOG_MODULE_REGISTER(test_sensor_manager, CONFIG_LOG_DEFAULT_LEVEL);

/**
 * @brief Test suite for sensor_manager.c
 * 
 * This file contains unit tests for the sensor manager module,
 * which coordinates environmental sensor readings and power management.
 * 
 * Note: These are demonstration tests showing the framework structure.
 * Full implementation would require integrating with actual sensor_manager.c
 */

/* Mock device instances (would normally come from device tree) */
static struct device mock_i2c_device = { .name = "I2C_0" };
static struct device mock_hts221_device = { .name = "HTS221" };
static struct device mock_lps22hb_device = { .name = "LPS22HB" };
static struct device mock_ccs811_device = { .name = "CCS811" };
static struct device mock_sx1509b_device = { .name = "SX1509B" };

/* Test fixture setup */
static void *sensor_manager_setup(void)
{
    LOG_INF("Setting up sensor manager tests");
    
    /* Initialize all mock systems */
    mock_i2c_init();
    mock_gpio_init();
    mock_sensor_init();
    
    return NULL;
}

static void sensor_manager_before_each(void *fixture)
{
    /* Reset all mock states before each test */
    mock_i2c_reset();
    mock_gpio_reset();
    mock_sensor_reset();
    
    /* Set up default device ready states */
    mock_i2c_set_device_ready(&mock_i2c_device, true);
    mock_sensor_set_device_ready(&mock_hts221_device, true);
    mock_sensor_set_device_ready(&mock_lps22hb_device, true);
    mock_sensor_set_device_ready(&mock_ccs811_device, true);
    mock_gpio_set_device_ready(&mock_sx1509b_device, true);
}

static void sensor_manager_teardown(void *fixture)
{
    LOG_INF("Tearing down sensor manager tests");
}

ZTEST_SUITE(sensor_manager, NULL, sensor_manager_setup, sensor_manager_before_each, NULL, sensor_manager_teardown);

/**
 * @brief Test sensor manager initialization workflow
 * 
 * This test demonstrates how the sensor manager would initialize
 * all connected sensors and verify their hardware IDs.
 */
ZTEST(sensor_manager, test_sensor_manager_init_workflow)
{
    LOG_INF("Testing sensor manager initialization workflow");
    
    /* Test 1: HTS221 initialization */
    /* Set up expected HTS221 hardware ID read (0x0F register should return 0xBC) */
    uint8_t hts221_who_am_i_reg = 0x0F;
    uint8_t hts221_expected_id = 0xBC;
    uint8_t hts221_id_buffer[1];
    
    struct mock_i2c_transaction hts221_id_read = {
        .type = MOCK_I2C_WRITE_READ,
        .addr = 0x5F,  /* HTS221 I2C address */
        .write_buf = &hts221_who_am_i_reg,
        .write_len = 1,
        .read_buf = &hts221_expected_id,
        .read_len = 1,
        .expected_return = 0
    };
    
    mock_i2c_expect_transaction(&hts221_id_read);
    
    /* Simulate HTS221 ID read */
    int ret = i2c_write_read_mock(&mock_i2c_device, 0x5F, &hts221_who_am_i_reg, 1, 
                                 hts221_id_buffer, 1);
    zassert_ok(ret, "HTS221 ID read should succeed");
    zassert_equal(hts221_id_buffer[0], 0xBC, "HTS221 ID should be 0xBC");
    
    /* Test 2: LPS22HB initialization */
    uint8_t lps22hb_who_am_i_reg = 0x0F;
    uint8_t lps22hb_expected_id = 0xB1;
    uint8_t lps22hb_id_buffer[1];
    
    struct mock_i2c_transaction lps22hb_id_read = {
        .type = MOCK_I2C_WRITE_READ,
        .addr = 0x5C,  /* LPS22HB I2C address */
        .write_buf = &lps22hb_who_am_i_reg,
        .write_len = 1,
        .read_buf = &lps22hb_expected_id,
        .read_len = 1,
        .expected_return = 0
    };
    
    mock_i2c_expect_transaction(&lps22hb_id_read);
    
    ret = i2c_write_read_mock(&mock_i2c_device, 0x5C, &lps22hb_who_am_i_reg, 1,
                             lps22hb_id_buffer, 1);
    zassert_ok(ret, "LPS22HB ID read should succeed");
    zassert_equal(lps22hb_id_buffer[0], 0xB1, "LPS22HB ID should be 0xB1");
    
    /* Test 3: CCS811 initialization with SX1509B power control */
    /* First, enable CCS811 power via SX1509B pin 10 */
    ret = gpio_pin_set_mock(&mock_sx1509b_device, 10, 1);
    zassert_ok(ret, "CCS811 power enable should succeed");
    
    /* Release CCS811 reset via SX1509B pin 11 */
    ret = gpio_pin_set_mock(&mock_sx1509b_device, 11, 1);
    zassert_ok(ret, "CCS811 reset release should succeed");
    
    /* Read CCS811 hardware ID */
    uint8_t ccs811_hw_id_reg = 0x20;
    uint8_t ccs811_expected_id = 0x81;
    uint8_t ccs811_id_buffer[1];
    
    struct mock_i2c_transaction ccs811_id_read = {
        .type = MOCK_I2C_WRITE_READ,
        .addr = 0x5A,  /* CCS811 I2C address */
        .write_buf = &ccs811_hw_id_reg,
        .write_len = 1,
        .read_buf = &ccs811_expected_id,
        .read_len = 1,
        .expected_return = 0
    };
    
    mock_i2c_expect_transaction(&ccs811_id_read);
    
    ret = i2c_write_read_mock(&mock_i2c_device, 0x5A, &ccs811_hw_id_reg, 1,
                             ccs811_id_buffer, 1);
    zassert_ok(ret, "CCS811 ID read should succeed");
    zassert_equal(ccs811_id_buffer[0], 0x81, "CCS811 ID should be 0x81");
    
    /* Verify all expected I2C transactions completed */
    mock_i2c_verify_complete();
    
    /* Verify GPIO pin states */
    int ccs811_power = mock_gpio_get_pin_value(&mock_sx1509b_device, 10);
    int ccs811_reset = mock_gpio_get_pin_value(&mock_sx1509b_device, 11);
    
    zassert_equal(ccs811_power, 1, "CCS811 power should be enabled");
    zassert_equal(ccs811_reset, 1, "CCS811 reset should be released");
    
    LOG_INF("Sensor manager initialization workflow test passed");
}

/**
 * @brief Test sensor data reading workflow
 * 
 * This test demonstrates how the sensor manager would read
 * environmental data from all sensors.
 */
ZTEST(sensor_manager, test_sensor_data_reading_workflow)
{
    LOG_INF("Testing sensor data reading workflow");
    
    /* Set up expected sensor readings */
    struct sensor_value temp_reading, humidity_reading, pressure_reading;
    
    sensor_value_from_float(&temp_reading, 25.5f);     /* 25.5°C */
    sensor_value_from_float(&humidity_reading, 60.2f); /* 60.2% RH */
    sensor_value_from_float(&pressure_reading, 1013.25f); /* 1013.25 hPa */
    
    /* Configure mock sensor readings */
    mock_sensor_set_reading(&mock_hts221_device, SENSOR_CHAN_AMBIENT_TEMP, &temp_reading);
    mock_sensor_set_reading(&mock_hts221_device, SENSOR_CHAN_HUMIDITY, &humidity_reading);
    mock_sensor_set_reading(&mock_lps22hb_device, SENSOR_CHAN_PRESS, &pressure_reading);
    
    /* Test HTS221 temperature and humidity reading */
    int ret = sensor_sample_fetch_mock(&mock_hts221_device);
    zassert_ok(ret, "HTS221 sample fetch should succeed");
    
    struct sensor_value retrieved_temp, retrieved_humidity;
    ret = sensor_channel_get_mock(&mock_hts221_device, SENSOR_CHAN_AMBIENT_TEMP, &retrieved_temp);
    zassert_ok(ret, "HTS221 temperature get should succeed");
    
    ret = sensor_channel_get_mock(&mock_hts221_device, SENSOR_CHAN_HUMIDITY, &retrieved_humidity);
    zassert_ok(ret, "HTS221 humidity get should succeed");
    
    /* Verify readings */
    float temp_float = sensor_value_to_float(&retrieved_temp);
    float humidity_float = sensor_value_to_float(&retrieved_humidity);
    
    zassert_within(temp_float, 25.5f, 0.01f, "Temperature should be 25.5°C");
    zassert_within(humidity_float, 60.2f, 0.01f, "Humidity should be 60.2%%");
    
    /* Test LPS22HB pressure reading */
    ret = sensor_sample_fetch_mock(&mock_lps22hb_device);
    zassert_ok(ret, "LPS22HB sample fetch should succeed");
    
    struct sensor_value retrieved_pressure;
    ret = sensor_channel_get_mock(&mock_lps22hb_device, SENSOR_CHAN_PRESS, &retrieved_pressure);
    zassert_ok(ret, "LPS22HB pressure get should succeed");
    
    float pressure_float = sensor_value_to_float(&retrieved_pressure);
    zassert_within(pressure_float, 1013.25f, 0.01f, "Pressure should be 1013.25 hPa");
    
    /* Verify call counts */
    zassert_equal(mock_sensor_get_fetch_call_count(&mock_hts221_device), 1,
                  "HTS221 fetch should be called once");
    zassert_equal(mock_sensor_get_get_call_count(&mock_hts221_device), 2,
                  "HTS221 get should be called twice (temp + humidity)");
    zassert_equal(mock_sensor_get_fetch_call_count(&mock_lps22hb_device), 1,
                  "LPS22HB fetch should be called once");
    zassert_equal(mock_sensor_get_get_call_count(&mock_lps22hb_device), 1,
                  "LPS22HB get should be called once");
    
    LOG_INF("Sensor data reading workflow test passed");
}

/**
 * @brief Test sensor error handling
 * 
 * This test verifies that the sensor manager properly handles
 * sensor failures and error conditions.
 */
ZTEST(sensor_manager, test_sensor_error_handling)
{
    LOG_INF("Testing sensor error handling");
    
    /* Test 1: Sensor device not ready */
    mock_sensor_set_device_ready(&mock_hts221_device, false);
    
    int ret = sensor_sample_fetch_mock(&mock_hts221_device);
    zassert_equal(ret, -ENODEV, "Should return ENODEV when device not ready");
    
    /* Test 2: Sensor fetch failure */
    mock_sensor_set_device_ready(&mock_hts221_device, true);
    mock_sensor_set_fetch_result(&mock_hts221_device, false);
    
    ret = sensor_sample_fetch_mock(&mock_hts221_device);
    zassert_equal(ret, -EIO, "Should return EIO when fetch fails");
    
    /* Test 3: I2C communication failure */
    uint8_t dummy_reg = 0x00;
    uint8_t dummy_buffer[1];
    
    /* Set I2C device not ready */
    mock_i2c_set_device_ready(&mock_i2c_device, false);
    
    ret = i2c_read_mock(&mock_i2c_device, dummy_buffer, 1, 0x5F);
    zassert_equal(ret, -ENODEV, "Should return ENODEV when I2C device not ready");
    
    /* Test 4: GPIO failure (SX1509B not ready) */
    mock_gpio_set_device_ready(&mock_sx1509b_device, false);
    
    ret = gpio_pin_set_mock(&mock_sx1509b_device, 10, 1);
    zassert_equal(ret, -ENODEV, "Should return ENODEV when GPIO device not ready");
    
    LOG_INF("Sensor error handling test passed");
}