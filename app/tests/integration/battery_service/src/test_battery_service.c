/*
 * Copyright (c) 2026 Eaton
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Integration coverage test for app/src/battery_service.c.
 *
 * Drives the production module against zephyr,adc-emul + voltage-divider
 * + gpio-emul. Uses adc_emul_const_value_set() to inject canned ADC
 * readings so the voltage-to-percentage conversion path runs end-to-end.
 */

#include <zephyr/ztest.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/adc/adc_emul.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>

#include "battery_service.h"

#define ADC_NODE        DT_NODELABEL(adc0)
#define VBATT_NODE      DT_PATH(vbatt)
#define CHARGE_GPIO_PIN 17

static const struct device *adc_dev = DEVICE_DT_GET(ADC_NODE);
static const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio_emul_node));

static void *suite_setup(void)
{
	/* Seed a plausible ADC reading and run init exactly once. The
	 * production module guards against double-init and keeps state in
	 * file-scope variables, so subsequent tests share the same
	 * initialised module.
	 */
	int err = adc_emul_const_value_set(adc_dev, 0, 450);
	zassert_ok(err, "adc_emul seed: %d", err);

	err = battery_service_init();
	zassert_ok(err, "battery_service_init: %d", err);
	return NULL;
}

static void set_battery_mv(uint32_t battery_mv)
{
	/* Voltage divider in the overlay scales by output / full =
	 * 180k / (180k + 1.5M) = ~10.71%. Inject the divided voltage so the
	 * voltage-divider driver scales it back up.
	 */
	uint32_t adc_input_mv = (battery_mv * 180U) / (180U + 1500U);
	int err = adc_emul_const_value_set(adc_dev, 0, adc_input_mv);
	zassert_ok(err, "adc_emul_const_value_set: %d", err);
}

static int charging_cb_count;
static bool charging_cb_last_state;
static void charging_cb(bool charging)
{
	charging_cb_count++;
	charging_cb_last_state = charging;
}

ZTEST(battery_service_integration, test_init_and_sample_full)
{
	zassert_true(device_is_ready(adc_dev), "adc emul not ready");
	zassert_true(device_is_ready(gpio_dev), "gpio emul not ready");

	/* Drive a "full battery" reading and verify clamping to 100%. The
	 * voltage-divider scaling round-trip plus the 12-bit ADC quantisation
	 * loses about 1% near the rails, so push slightly above the full
	 * threshold (4200 mV) to land on 100%.
	 */
	set_battery_mv(4250);

	int ret = battery_service_sample();
	zassert_ok(ret, "battery_service_sample: %d", ret);

	int level = battery_service_get_level();
	zassert_equal(level, 100, ">= 4200 mV should clamp to 100%%, got %d", level);
}

ZTEST(battery_service_integration, test_voltage_to_percentage_curve)
{
	/* Mid-range and empty paths through voltage_to_percentage(). */
	set_battery_mv(3600);
	zassert_ok(battery_service_sample(), "sample at 3600 mV");
	int mid = battery_service_get_level();
	zassert_true(mid > 0 && mid < 100, "mid-range expected, got %d", mid);

	set_battery_mv(2900);
	zassert_ok(battery_service_sample(), "sample at 2900 mV");
	int empty = battery_service_get_level();
	zassert_equal(empty, 0, "<= 3000 mV should be 0%%, got %d", empty);
}

ZTEST(battery_service_integration, test_charging_status_and_callback)
{
	battery_service_register_charging_callback(charging_cb);
	charging_cb_count = 0;

	/* charge_status is active-low: drive the line low to indicate charging. */
	int err = gpio_emul_input_set(gpio_dev, CHARGE_GPIO_PIN, 0);
	zassert_ok(err, "gpio_emul_input_set low: %d", err);
	zassert_true(battery_service_is_charging(),
		     "expected charging when pin pulled low");

	err = gpio_emul_input_set(gpio_dev, CHARGE_GPIO_PIN, 1);
	zassert_ok(err, "gpio_emul_input_set high: %d", err);
	zassert_false(battery_service_is_charging(),
		      "expected not-charging when pin pulled high");

	/* The is_charging() polling path invokes the callback on transitions;
	 * we forced two transitions above.
	 */
	zassert_true(charging_cb_count >= 1, "callback should have fired");
}

ZTEST_SUITE(battery_service_integration, NULL, suite_setup, NULL, NULL, NULL);
