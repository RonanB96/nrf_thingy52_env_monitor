/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(test_hil_battery, LOG_LEVEL_INF);

ZTEST_SUITE(hil_battery, NULL, NULL, NULL, NULL, NULL);

/*
 * Battery ADC path: vbatt voltage-divider node uses SAADC AIN4 (P0.28) with
 * BAT_MON_EN power gate (SX1509B pin 4). Tests that the full ADC chain works.
 */
ZTEST(hil_battery, test_vbatt_adc)
{
	const struct device *dev = DEVICE_DT_GET(DT_PATH(vbatt));

	zassert_true(device_is_ready(dev), "vbatt voltage-divider not ready");
	zassert_ok(sensor_sample_fetch(dev), "vbatt sample fetch failed");

	struct sensor_value voltage;

	zassert_ok(sensor_channel_get(dev, SENSOR_CHAN_VOLTAGE, &voltage),
		   "vbatt channel get failed");

	/* voltage-divider driver reports in volts; LiPo range 3.0–4.3 V */
	double v = sensor_value_to_double(&voltage);

	LOG_INF("Battery voltage: %.3f V", v);
	zassert_true(v > 3.0 && v < 4.3, "Battery voltage %.3f V out of range (3.0..4.3)", v);
}
