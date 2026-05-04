/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "lps22hb.h"

LOG_MODULE_REGISTER(test_hil_lps22hb, LOG_LEVEL_INF);

ZTEST_SUITE(hil_lps22hb, NULL, NULL, NULL, NULL, NULL);

ZTEST(hil_lps22hb, test_pressure_in_range)
{
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(lps22hb_press));

	zassert_true(device_is_ready(dev), "LPS22HB not ready");
	zassert_ok(sensor_sample_fetch(dev), "LPS22HB sample fetch failed");

	struct sensor_value press;

	zassert_ok(sensor_channel_get(dev, SENSOR_CHAN_PRESS, &press),
		   "LPS22HB pressure read failed");

	/* Zephyr reports pressure in kPa; plausible range is 90–115 kPa */
	double p = sensor_value_to_double(&press);

	LOG_INF("LPS22HB pressure: %.2f kPa", p);
	zassert_true(p > 90.0 && p < 115.0, "Pressure %.2f kPa out of range (90..115)", p);
}

/*
 * LPS22HB DRDY interrupt path (P0.23).
 * The production driver uses a raw gpio_add_callback on this pin — not the
 * Zephyr sensor trigger API. This test exercises that same GPIO interrupt path
 * by triggering a one-shot conversion and waiting for the DRDY edge.
 */
static struct k_sem lps22hb_drdy_sem;

static void lps22hb_int_cb(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(port);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);
	k_sem_give(&lps22hb_drdy_sem);
}

ZTEST(hil_lps22hb, test_drdy_interrupt)
{
	const struct device *i2c = DEVICE_DT_GET(DT_NODELABEL(i2c0));
	static const struct gpio_dt_spec int_pin =
		GPIO_DT_SPEC_GET(DT_NODELABEL(lps22hb_int), gpios);
	static struct gpio_callback gpio_cb;

	zassert_true(device_is_ready(i2c), "I2C0 not ready");
	zassert_true(device_is_ready(int_pin.port), "GPIO not ready");

	k_sem_init(&lps22hb_drdy_sem, 0, 1);

	zassert_ok(gpio_pin_configure_dt(&int_pin, GPIO_INPUT), "GPIO configure failed");
	gpio_init_callback(&gpio_cb, lps22hb_int_cb, BIT(int_pin.pin));
	zassert_ok(gpio_add_callback(int_pin.port, &gpio_cb), "GPIO add callback failed");
	zassert_ok(gpio_pin_interrupt_configure_dt(&int_pin, GPIO_INT_EDGE_RISING),
		   "GPIO interrupt configure failed");

	/* Read-modify-write: set only the bits we need, preserve driver-configured bits */
	uint8_t ctrl3_orig = 0;
	uint8_t ctrl2_orig = 0;
	uint8_t addr = DT_REG_ADDR(DT_NODELABEL(lps22hb_press));

	zassert_ok(i2c_reg_read_byte(i2c, addr, LPS22HB_REG_CTRL_REG3, &ctrl3_orig),
		   "LPS22HB CTRL_REG3 read failed");
	zassert_ok(i2c_reg_read_byte(i2c, addr, LPS22HB_REG_CTRL_REG2, &ctrl2_orig),
		   "LPS22HB CTRL_REG2 read failed");

	zassert_ok(i2c_reg_write_byte(i2c, addr, LPS22HB_REG_CTRL_REG3,
				      ctrl3_orig | LPS22HB_MASK_CTRL_REG3_DRDY),
		   "LPS22HB CTRL_REG3 write failed");
	zassert_ok(i2c_reg_write_byte(i2c, addr, LPS22HB_REG_CTRL_REG2,
				      ctrl2_orig | LPS22HB_MASK_CTRL_REG2_ONE_SHOT),
		   "LPS22HB one-shot trigger failed");

	int ret = k_sem_take(&lps22hb_drdy_sem, K_MSEC(500));

	gpio_pin_interrupt_configure_dt(&int_pin, GPIO_INT_DISABLE);
	gpio_remove_callback(int_pin.port, &gpio_cb);
	/* Restore registers to state the Zephyr driver left them in */
	i2c_reg_write_byte(i2c, addr, LPS22HB_REG_CTRL_REG3, ctrl3_orig);
	i2c_reg_write_byte(i2c, addr, LPS22HB_REG_CTRL_REG2,
			   ctrl2_orig & ~LPS22HB_MASK_CTRL_REG2_ONE_SHOT);

	zassert_equal(ret, 0, "LPS22HB DRDY interrupt on P0.23 did not fire within 500ms");
	LOG_INF("LPS22HB DRDY interrupt fired");
}
