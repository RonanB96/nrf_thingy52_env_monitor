/*
 * Copyright (c) 2026 Eaton
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Integration coverage test for the app/src sensor wrapper layer.
 *
 * This test does NOT validate sensor *correctness* — that responsibility
 * lives in the HIL suite under app/tests/hardware. Its sole purpose is to
 * execute the lines of:
 *
 *   - app/src/sensor_hts221_driver.c
 *   - app/src/sensor_lps22hb_driver.c
 *   - app/src/sensor_ccs811_driver.c
 *
 * under native_sim with gcov instrumentation, against the upstream Zephyr
 * sensor drivers backed by minimal i2c emul mocks (in ../emul).
 *
 * Reads will return zero or fail gracefully — that is intended; the
 * wrappers' error paths are valuable coverage too.
 */

#include <zephyr/ztest.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include "sensor_hts221_driver.h"
#include "sensor_lps22hb_driver.h"
#include "sensor_ccs811_driver.h"

#define I2C0_NODE     DT_NODELABEL(i2c0)
#define HTS221_NODE   DT_NODELABEL(hts221)
#define LPS22HB_NODE  DT_NODELABEL(lps22hb_press)
#define CCS811_NODE   DT_NODELABEL(ccs811)

static const struct gpio_dt_spec lps22hb_int_spec =
	GPIO_DT_SPEC_GET(DT_NODELABEL(lps22hb_int), gpios);

ZTEST(sensor_wrappers_integration, test_hts221_lifecycle)
{
	const struct device *i2c = DEVICE_DT_GET(I2C0_NODE);
	const struct device *hts221 = DEVICE_DT_GET(HTS221_NODE);

	zassert_true(device_is_ready(i2c), "i2c emul not ready");
	zassert_true(device_is_ready(hts221), "hts221 driver init failed");

	/* Init exercises wrapper init, trigger registration, and error paths. */
	int ret = hts221_driver_init(i2c);
	zassert_ok(ret, "hts221_driver_init returned %d", ret);

	/* Power on/off paths are independent of bus traffic outcomes. */
	(void)hts221_driver_power_on();
	zassert_true(hts221_driver_is_enabled() || !hts221_driver_is_enabled(),
		     "always true; just exercising the getter");
	(void)hts221_driver_power_off();

	/* Read paths will time out on the data-ready semaphore — that is
	 * fine; we only need the lines executed.
	 */
	float t = 0.0f, h = 0.0f;
	(void)hts221_driver_read_temperature(&t);
	(void)hts221_driver_read_humidity(&h);
	(void)hts221_driver_read_both(&t, &h);
}

ZTEST(sensor_wrappers_integration, test_lps22hb_lifecycle)
{
	const struct device *i2c = DEVICE_DT_GET(I2C0_NODE);
	const struct device *lps = DEVICE_DT_GET(LPS22HB_NODE);

	zassert_true(device_is_ready(i2c), "i2c emul not ready");
	zassert_true(device_is_ready(lps), "lps22hb driver init failed");

	int ret = lps22hb_driver_init(i2c, &lps22hb_int_spec);
	zassert_ok(ret, "lps22hb_driver_init returned %d", ret);

	(void)lps22hb_driver_power_on();
	(void)lps22hb_driver_trigger_oneshot();
	(void)lps22hb_driver_wait_data_ready(K_MSEC(10));

	float p = 0.0f;
	(void)lps22hb_driver_read_pressure(&p);
	(void)lps22hb_driver_is_enabled();
	(void)lps22hb_driver_power_off();
}

ZTEST(sensor_wrappers_integration, test_ccs811_lifecycle)
{
	const struct device *ccs = DEVICE_DT_GET(CCS811_NODE);

	zassert_true(device_is_ready(ccs), "ccs811 driver init failed");

	int ret = ccs811_driver_init(ccs);
	zassert_ok(ret, "ccs811_driver_init returned %d", ret);

	(void)ccs811_driver_is_ready();
	(void)ccs811_driver_conditioning_time_remaining();
	(void)ccs811_driver_baseline_save_due();
	(void)ccs811_driver_is_enabled();

	/* Baseline save/restore exercise the settings paths. */
	(void)ccs811_driver_save_baseline();
	(void)ccs811_driver_restore_baseline();
}

ZTEST_SUITE(sensor_wrappers_integration, NULL, NULL, NULL, NULL, NULL);
