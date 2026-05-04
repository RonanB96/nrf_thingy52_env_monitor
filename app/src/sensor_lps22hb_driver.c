/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "sensor_lps22hb_driver.h"
#include <zephyr/logging/log.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <stdbool.h>

/* Include LPS22HB driver header for register definitions */
#include "lps22hb.h"

LOG_MODULE_REGISTER(lps22hb_driver, CONFIG_LOG_DEFAULT_LEVEL);

#define LPS22HB_I2C_ADDR 0x5C

/* Device tree node */
#define LPS22HB_NODE DT_NODELABEL(lps22hb_press)

/* Additional LPS22HB status register masks */
static const uint8_t LPS22HB_DRDY_PRESS_MASK = 0x01U; /* Pressure data available */
static const uint8_t LPS22HB_DRDY_TEMP_MASK = 0x02U;  /* Temperature data available */
static const uint32_t LPS22HB_STABILIZATION_DELAY_MS = 50U;
static const uint8_t LPS22HB_PRESSURE_REG_LEN = 5U; /* PRESS_OUT_XL/L/H + TEMP_OUT_L/H bytes */
static const uint32_t LPS22HB_DRDY_TIMEOUT_MS = 100U;

/* Static state */
static const struct device *lps22hb_dev = NULL;
static const struct device *i2c_dev = NULL;
static struct gpio_dt_spec lps22hb_int_pin;
static bool lps22hb_enabled = false;
static bool interrupt_initialized = false;

/* Data ready semaphore and callback */
static struct k_sem data_ready_sem;
static struct gpio_callback lps22hb_cb_data;

/* LPS22HB Data Ready Interrupt Handler */
static void lps22hb_drdy_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	(void)dev;
	(void)cb;
	(void)pins;
	k_sem_give(&data_ready_sem);
}

int lps22hb_driver_init(const struct device *i2c_device, const struct gpio_dt_spec *int_spec)
{
	int ret;
	uint8_t ctrl_reg1;

	if (!i2c_device || !device_is_ready(i2c_device)) {
		LOG_ERR("Invalid or not ready I2C device");
		return -ENODEV;
	}

	/* Get LPS22HB device from device tree */
	lps22hb_dev = DEVICE_DT_GET(LPS22HB_NODE);
	if (!lps22hb_dev || !device_is_ready(lps22hb_dev)) {
		LOG_ERR("LPS22HB device not found or not ready");
		return -ENODEV;
	}

	if (!int_spec) {
		LOG_ERR("Invalid interrupt GPIO spec");
		return -EINVAL;
	}

	i2c_dev = i2c_device;
	lps22hb_int_pin = *int_spec;
	lps22hb_enabled = false;
	interrupt_initialized = false;

	/* Initialize data ready semaphore */
	k_sem_init(&data_ready_sem, 0, 1);

	/* Initialize interrupt if GPIO is ready */
	if (device_is_ready(lps22hb_int_pin.port)) {
		/* Configure LPS22HB interrupt pin */
		ret = gpio_pin_configure_dt(&lps22hb_int_pin, GPIO_INPUT);
		if (ret < 0) {
			LOG_WRN("Failed to configure LPS22HB INT pin: %d", ret);
			return ret;
		}

		ret = gpio_pin_interrupt_configure_dt(&lps22hb_int_pin, GPIO_INT_DISABLE);
		if (ret < 0) {
			LOG_WRN("Failed to configure LPS22HB INT interrupt: %d", ret);
			return ret;
		}

		gpio_init_callback(&lps22hb_cb_data, lps22hb_drdy_callback,
				   BIT(lps22hb_int_pin.pin));
		ret = gpio_add_callback(lps22hb_int_pin.port, &lps22hb_cb_data);
		if (ret < 0) {
			LOG_WRN("Failed to add LPS22HB INT callback: %d", ret);
			return ret;
		}

		interrupt_initialized = true;
		LOG_INF("LPS22HB driver initialized with interrupt support");
	} else {
		LOG_WRN("LPS22HB interrupt GPIO not ready - using polling mode");
	}

	/* Read current CTRL_REG1 */
	ret = i2c_reg_read_byte(i2c_dev, LPS22HB_I2C_ADDR, LPS22HB_REG_CTRL_REG1, &ctrl_reg1);
	if (ret < 0) {
		LOG_ERR("Failed to read LPS22HB CTRL_REG1: %d", ret);
		return ret;
	}

	/* Clear ODR bits (one shot mode)  */
	ctrl_reg1 &= ~LPS22HB_MASK_CTRL_REG1_ODR;

	ret = i2c_reg_write_byte(i2c_dev, LPS22HB_I2C_ADDR, LPS22HB_REG_CTRL_REG1, ctrl_reg1);
	if (ret < 0) {
		LOG_ERR("Failed to set LPS22HB ODR: %d", ret);
		return ret;
	}

	return 0;
}

int lps22hb_driver_power_on(void)
{
	if (!i2c_dev) {
		LOG_ERR("LPS22HB driver not initialized");
		return -ENODEV;
	}

	uint8_t res_conf;
	uint8_t ctrl_reg3;
	int ret;

	/* Enable sensor by clearing LC_EN bit (normal current mode) */
	ret = i2c_reg_read_byte(i2c_dev, LPS22HB_I2C_ADDR, LPS22HB_REG_RES_CONF, &res_conf);
	if (ret < 0) {
		LOG_ERR("Failed to read LPS22HB RES_CONF: %d", ret);
		return ret;
	}

	res_conf &= ~LPS22HB_SHIFT_RES_CONF_LC_EN; /* Clear LC_EN = normal current mode */

	ret = i2c_reg_write_byte(i2c_dev, LPS22HB_I2C_ADDR, LPS22HB_REG_RES_CONF, res_conf);
	if (ret < 0) {
		LOG_ERR("Failed to enable LPS22HB (clear LC_EN): %d", ret);
		return ret;
	}

	/* Configure CTRL_REG3 to enable data ready interrupt if supported */
	if (interrupt_initialized) {
		ret = i2c_reg_read_byte(i2c_dev, LPS22HB_I2C_ADDR, LPS22HB_REG_CTRL_REG3,
					&ctrl_reg3);
		if (ret < 0) {
			LOG_ERR("Failed to read LPS22HB CTRL_REG3: %d", ret);
			return ret;
		}

		/* Enable DRDY signal on INT_DRDY pin (active high, push-pull) */
		ctrl_reg3 |= LPS22HB_MASK_CTRL_REG3_DRDY;

		ret = i2c_reg_write_byte(i2c_dev, LPS22HB_I2C_ADDR, LPS22HB_REG_CTRL_REG3,
					 ctrl_reg3);
		if (ret < 0) {
			LOG_ERR("Failed to enable LPS22HB DRDY interrupt: %d", ret);
			return ret;
		}

		LOG_DBG("LPS22HB powered on with DRDY interrupt enabled");
	} else {
		LOG_DBG("LPS22HB powered on in polling mode");
	}

	lps22hb_enabled = true;
	return 0;
}

int lps22hb_driver_trigger_oneshot(void)
{
	if (!i2c_dev) {
		LOG_ERR("LPS22HB driver not initialized");
		return -ENODEV;
	}

	uint8_t ctrl_reg2;
	int ret;

	/* Clear any existing data ready status by reading STATUS and data registers */
	uint8_t status_reg;
	ret = i2c_reg_read_byte(i2c_dev, LPS22HB_I2C_ADDR, LPS22HB_REG_STATUS, &status_reg);
	if (ret == 0 && (status_reg & (LPS22HB_DRDY_PRESS_MASK | LPS22HB_DRDY_TEMP_MASK))) {
		/* Clear by reading pressure and temperature data */
		uint8_t dummy_data[LPS22HB_PRESSURE_REG_LEN];
		i2c_burst_read(i2c_dev, LPS22HB_I2C_ADDR, LPS22HB_REG_PRESS_OUT_XL, dummy_data,
			       LPS22HB_PRESSURE_REG_LEN);
		LOG_DBG("Cleared existing LPS22HB data ready status: 0x%02x", status_reg);
	}

	/* Reset semaphore and enable interrupt for this measurement */
	k_sem_reset(&data_ready_sem);
	if (interrupt_initialized) {
		ret = gpio_pin_interrupt_configure_dt(&lps22hb_int_pin, GPIO_INT_EDGE_RISING);
		if (ret != 0) {
			LOG_WRN("Failed to enable LPS22HB interrupt: %d", ret);
		}
	}

	/* Read current CTRL_REG2 */
	ret = i2c_reg_read_byte(i2c_dev, LPS22HB_I2C_ADDR, LPS22HB_REG_CTRL_REG2, &ctrl_reg2);
	if (ret < 0) {
		LOG_ERR("Failed to read LPS22HB CTRL_REG2: %d", ret);
		return ret;
	}

	/* Set ONE_SHOT bit to trigger measurement */
	ctrl_reg2 |= LPS22HB_MASK_CTRL_REG2_ONE_SHOT;

	ret = i2c_reg_write_byte(i2c_dev, LPS22HB_I2C_ADDR, LPS22HB_REG_CTRL_REG2, ctrl_reg2);
	if (ret < 0) {
		LOG_ERR("Failed to trigger LPS22HB one-shot: %d", ret);
		return ret;
	}

	LOG_DBG("LPS22HB one-shot measurement triggered");
	return 0;
}

int lps22hb_driver_wait_data_ready(k_timeout_t timeout)
{
	if (!interrupt_initialized) {
		/* Polling mode - just wait the expected conversion time */
		k_sleep(timeout);
		return 0;
	}

	/* Wait for data ready interrupt using semaphore */
	int64_t start_time = k_uptime_get();
	int ret = k_sem_take(&data_ready_sem, timeout);

	/* Disable interrupt after measurement */
	gpio_pin_interrupt_configure_dt(&lps22hb_int_pin, GPIO_INT_DISABLE);

	if (ret != 0) {
		LOG_WRN("LPS22HB data ready timeout after %lld ms", k_uptime_get() - start_time);
		return -ETIMEDOUT;
	} else {
		LOG_DBG("LPS22HB data ready after %lld ms", k_uptime_get() - start_time);
		return 0;
	}
}

int lps22hb_driver_power_off(void)
{
	if (!i2c_dev) {
		LOG_ERR("LPS22HB driver not initialized");
		return -ENODEV;
	}

	uint8_t res_conf;
	int ret;

	/* Disable interrupt if enabled */
	if (interrupt_initialized) {
		gpio_pin_interrupt_configure_dt(&lps22hb_int_pin, GPIO_INT_DISABLE);
	}

	/* Power down sensor by setting LC_EN bit (low current mode) */
	ret = i2c_reg_read_byte(i2c_dev, LPS22HB_I2C_ADDR, LPS22HB_REG_RES_CONF, &res_conf);
	if (ret < 0) {
		LOG_ERR("Failed to read LPS22HB RES_CONF: %d", ret);
		return ret;
	}

	res_conf |= LPS22HB_SHIFT_RES_CONF_LC_EN; /* Set LC_EN = low current mode (~5µA) */

	ret = i2c_reg_write_byte(i2c_dev, LPS22HB_I2C_ADDR, LPS22HB_REG_RES_CONF, res_conf);
	if (ret < 0) {
		LOG_ERR("Failed to power down LPS22HB (set LC_EN): %d", ret);
		return ret;
	}

	lps22hb_enabled = false;
	LOG_DBG("LPS22HB powered down - ~5µA (LC_EN mode)");
	return 0;
}

bool lps22hb_driver_is_enabled(void)
{
	return lps22hb_enabled;
}

int lps22hb_driver_read_pressure(float *pressure)
{
	struct sensor_value press_val;
	int ret;

	if (!lps22hb_dev || !pressure) {
		return -EINVAL;
	}

	if (!device_is_ready(lps22hb_dev)) {
		LOG_WRN("LPS22HB device not ready");
		return -ENODEV;
	}

	/* Power on sensor */
	ret = lps22hb_driver_power_on();
	if (ret != 0) {
		LOG_ERR("Failed to power on LPS22HB: %d", ret);
		return ret;
	}

	/* Wait for sensor stabilization and trigger one-shot measurement */
	k_msleep((int32_t)LPS22HB_STABILIZATION_DELAY_MS);

	ret = lps22hb_driver_trigger_oneshot();
	if (ret != 0) {
		LOG_ERR("Failed to trigger LPS22HB one-shot: %d", ret);
		goto power_down;
	}

	/* Wait for data ready - uses interrupt semaphore if interrupt_initialized, else polls */
	ret = lps22hb_driver_wait_data_ready(K_MSEC(LPS22HB_DRDY_TIMEOUT_MS));
	if (ret != 0) {
		LOG_WRN("LPS22HB DRDY wait failed: %d", ret);
		goto power_down;
	}

	/* Fetch sample using Zephyr driver */
	ret = sensor_sample_fetch(lps22hb_dev);
	if (ret != 0) {
		LOG_ERR("Failed to fetch LPS22HB sample: %d", ret);
		goto power_down;
	}

	/* Read pressure */
	ret = sensor_channel_get(lps22hb_dev, SENSOR_CHAN_PRESS, &press_val);
	if (ret == 0) {
		*pressure = sensor_value_to_float(&press_val);
		LOG_DBG("Pressure: %.2f kPa", (double)*pressure);
	} else {
		LOG_WRN("Failed to read pressure: %d", ret);
		goto power_down;
	}

power_down:
	/* Always power down after reading */
	{
		int power_ret = lps22hb_driver_power_off();
		if (power_ret != 0) {
			LOG_WRN("Failed to power off LPS22HB: %d", power_ret);
		}
	}

	return ret;
}
