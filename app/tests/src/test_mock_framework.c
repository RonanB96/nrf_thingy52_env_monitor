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

LOG_MODULE_REGISTER(test_mocks, CONFIG_LOG_DEFAULT_LEVEL);

/**
 * @brief Test suite for mock framework validation
 *
 * This file contains tests to validate that the mock framework
 * implementations work correctly before testing actual modules.
 */

/* Test fixture setup */
static void *mock_tests_setup(void)
{
	LOG_INF("Setting up mock framework tests");

	/* Initialize all mock systems */
	mock_i2c_init();
	mock_gpio_init();
	mock_sensor_init();

	return NULL;
}

static void mock_tests_before_each(void *fixture)
{
	/* Reset all mock states before each test */
	mock_i2c_reset();
	mock_gpio_reset();
	mock_sensor_reset();
}

static void mock_tests_teardown(void *fixture)
{
	LOG_INF("Tearing down mock framework tests");
}

ZTEST_SUITE(mock_framework, NULL, mock_tests_setup, mock_tests_before_each, NULL,
	    mock_tests_teardown);

/**
 * @brief Test I2C mock basic functionality
 */
ZTEST(mock_framework, test_i2c_mock_basic)
{
	LOG_INF("Testing I2C mock basic functionality");

	/* Create a mock device */
	struct device mock_i2c_dev = {0};

	/* Set up expected I2C transaction */
	uint8_t write_data[] = {0x20, 0x01};
	uint8_t read_data[] = {0x81};
	uint8_t read_buffer[1];

	struct mock_i2c_transaction expected = {.type = MOCK_I2C_WRITE_READ,
						.addr = 0x3E,
						.write_buf = write_data,
						.write_len = sizeof(write_data),
						.read_buf = read_data, /* Expected data to return */
						.read_len = sizeof(read_data),
						.expected_return = 0};

	mock_i2c_expect_transaction(&expected);

	/* Simulate I2C operation that would be called by sensor driver */
	int ret = i2c_write_read_mock(&mock_i2c_dev, 0x3E, write_data, sizeof(write_data),
				      read_buffer, sizeof(read_buffer));

	/* Verify results */
	zassert_ok(ret, "I2C write_read should succeed");
	zassert_mem_equal(read_buffer, read_data, sizeof(read_data),
			  "Read data should match expected");

	/* Verify all expected transactions were processed */
	mock_i2c_verify_complete();

	LOG_INF("I2C mock test passed");
}

/**
 * @brief Test GPIO mock basic functionality
 */
ZTEST(mock_framework, test_gpio_mock_basic)
{
	LOG_INF("Testing GPIO mock basic functionality");

	/* Create a mock device */
	struct device mock_gpio_dev = {0};

	/* Test GPIO pin configuration */
	int ret = gpio_pin_configure_mock(&mock_gpio_dev, 10, GPIO_OUTPUT_ACTIVE);
	zassert_ok(ret, "GPIO pin configure should succeed");

	/* Test GPIO pin set */
	ret = gpio_pin_set_mock(&mock_gpio_dev, 10, 1);
	zassert_ok(ret, "GPIO pin set should succeed");

	/* Test GPIO pin get */
	int value = gpio_pin_get_mock(&mock_gpio_dev, 10);
	zassert_equal(value, 1, "GPIO pin value should be 1");

	/* Test GPIO pin toggle */
	ret = gpio_pin_toggle_mock(&mock_gpio_dev, 10);
	zassert_ok(ret, "GPIO pin toggle should succeed");

	value = gpio_pin_get_mock(&mock_gpio_dev, 10);
	zassert_equal(value, 0, "GPIO pin value should be 0 after toggle");

	/* Verify pin configuration */
	mock_gpio_verify_pin_config(&mock_gpio_dev, 10, GPIO_OUTPUT_ACTIVE);

	LOG_INF("GPIO mock test passed");
}

/**
 * @brief Test sensor mock basic functionality
 */
ZTEST(mock_framework, test_sensor_mock_basic)
{
	LOG_INF("Testing sensor mock basic functionality");

	/* Create a mock device */
	struct device mock_sensor_dev = {0};

	/* Set up expected sensor reading */
	struct sensor_value temp_reading;
	sensor_value_from_float(&temp_reading, 25.5f);

	mock_sensor_set_reading(&mock_sensor_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp_reading);

	/* Test sensor sample fetch */
	int ret = sensor_sample_fetch_mock(&mock_sensor_dev);
	zassert_ok(ret, "Sensor sample fetch should succeed");

	/* Test sensor channel get */
	struct sensor_value retrieved_value;
	ret = sensor_channel_get_mock(&mock_sensor_dev, SENSOR_CHAN_AMBIENT_TEMP, &retrieved_value);
	zassert_ok(ret, "Sensor channel get should succeed");

	/* Verify the value */
	float retrieved_float = sensor_value_to_float(&retrieved_value);
	zassert_within(retrieved_float, 25.5f, 0.01f, "Temperature reading should match");

	/* Test call counting */
	zassert_equal(mock_sensor_get_fetch_call_count(&mock_sensor_dev), 1,
		      "Fetch should have been called once");
	zassert_equal(mock_sensor_get_get_call_count(&mock_sensor_dev), 1,
		      "Get should have been called once");

	LOG_INF("Sensor mock test passed");
}

/**
 * @brief Test mock error conditions
 */
ZTEST(mock_framework, test_mock_error_conditions)
{
	LOG_INF("Testing mock error conditions");

	/* Create mock devices */
	struct device mock_i2c_dev = {0};
	struct device mock_gpio_dev = {0};
	struct device mock_sensor_dev = {0};

	/* Test I2C device not ready */
	mock_i2c_set_device_ready(&mock_i2c_dev, false);
	uint8_t dummy_data[] = {0x00};
	int ret = i2c_write_mock(&mock_i2c_dev, dummy_data, sizeof(dummy_data), 0x3E);
	zassert_equal(ret, -ENODEV, "I2C should return ENODEV when device not ready");

	/* Reset I2C device to ready for further tests */
	mock_i2c_set_device_ready(&mock_i2c_dev, true);

	/* Test GPIO device not ready */
	mock_gpio_set_device_ready(&mock_gpio_dev, false);
	ret = gpio_pin_configure_mock(&mock_gpio_dev, 0, GPIO_OUTPUT);
	zassert_equal(ret, -ENODEV, "GPIO should return ENODEV when device not ready");

	/* Reset GPIO device to ready for further tests */
	mock_gpio_set_device_ready(&mock_gpio_dev, true);

	/* Test sensor device not ready */
	mock_sensor_set_device_ready(&mock_sensor_dev, false);
	ret = sensor_sample_fetch_mock(&mock_sensor_dev);
	zassert_equal(ret, -ENODEV, "Sensor should return ENODEV when device not ready");

	/* Test sensor fetch failure */
	mock_sensor_set_device_ready(&mock_sensor_dev, true);
	mock_sensor_set_fetch_result(&mock_sensor_dev, false);
	ret = sensor_sample_fetch_mock(&mock_sensor_dev);
	zassert_equal(ret, -EIO, "Sensor should return EIO when fetch fails");

	LOG_INF("Mock error condition tests passed");
}

/**
 * @brief Test complex mock interaction scenarios
 */
ZTEST(mock_framework, test_mock_complex_scenarios)
{
	LOG_INF("Testing complex mock interaction scenarios");

	/* Scenario: CCS811 sensor initialization sequence */
	struct device mock_i2c_dev = {0};
	struct device mock_gpio_dev = {0};

	/* Step 1: GPIO power on (SX1509B pin 10) */
	int ret = gpio_pin_set_mock(&mock_gpio_dev, 10, 1);
	zassert_ok(ret, "GPIO power pin set should succeed");

	/* Step 2: GPIO reset release (SX1509B pin 11) */
	ret = gpio_pin_set_mock(&mock_gpio_dev, 11, 1);
	zassert_ok(ret, "GPIO reset pin set should succeed");

	/* Step 3: I2C Hardware ID read */
	uint8_t hw_id_reg = 0x20;
	uint8_t expected_hw_id = 0x81;
	uint8_t hw_id_buffer[1];

	struct mock_i2c_transaction hw_id_read = {.type = MOCK_I2C_WRITE_READ,
						  .addr = 0x5A,
						  .write_buf = &hw_id_reg,
						  .write_len = 1,
						  .read_buf = &expected_hw_id,
						  .read_len = 1,
						  .expected_return = 0};

	mock_i2c_expect_transaction(&hw_id_read);

	ret = i2c_write_read_mock(&mock_i2c_dev, 0x5A, &hw_id_reg, 1, hw_id_buffer, 1);
	zassert_ok(ret, "I2C hardware ID read should succeed");
	zassert_equal(hw_id_buffer[0], 0x81, "Hardware ID should be 0x81");

	/* Verify all mock interactions */
	mock_i2c_verify_complete();

	/* Verify GPIO states */
	int power_state = mock_gpio_get_pin_value(&mock_gpio_dev, 10);
	int reset_state = mock_gpio_get_pin_value(&mock_gpio_dev, 11);

	zassert_equal(power_state, 1, "Power pin should be high");
	zassert_equal(reset_state, 1, "Reset pin should be high");

	LOG_INF("Complex mock scenario test passed");
}