/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(test_hil_hts221, LOG_LEVEL_INF);

ZTEST_SUITE(hil_hts221, NULL, NULL, NULL, NULL, NULL);

ZTEST(hil_hts221, test_temperature_in_range)
{
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(hts221));

	zassert_true(device_is_ready(dev), "HTS221 not ready");
	zassert_ok(sensor_sample_fetch(dev), "HTS221 sample fetch failed");

	struct sensor_value temp;

	zassert_ok(sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp),
		   "HTS221 temperature read failed");

	double t = sensor_value_to_double(&temp);

	LOG_INF("HTS221 temperature: %.2f C", t);
	zassert_true(t > 0.0 && t < 50.0, "Temperature %.2f C out of range (0..50)", t);
}

ZTEST(hil_hts221, test_humidity_in_range)
{
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(hts221));

	zassert_true(device_is_ready(dev), "HTS221 not ready");
	zassert_ok(sensor_sample_fetch(dev), "HTS221 sample fetch failed");

	struct sensor_value humid;

	zassert_ok(sensor_channel_get(dev, SENSOR_CHAN_HUMIDITY, &humid),
		   "HTS221 humidity read failed");

	double h = sensor_value_to_double(&humid);

	LOG_INF("HTS221 humidity: %.2f %%", h);
	zassert_true(h >= 0.0 && h <= 100.0, "Humidity %.2f %% out of range (0..100)", h);
}

static struct k_sem hts221_drdy_sem;

static void hts221_drdy_handler(const struct device *dev, const struct sensor_trigger *trig)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(trig);
	k_sem_give(&hts221_drdy_sem);
}

ZTEST(hil_hts221, test_drdy_trigger)
{
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(hts221));
	struct sensor_trigger trig = {
		.type = SENSOR_TRIG_DATA_READY,
		.chan = SENSOR_CHAN_ALL,
	};

	zassert_true(device_is_ready(dev), "HTS221 not ready");

	k_sem_init(&hts221_drdy_sem, 0, 1);

	zassert_ok(sensor_trigger_set(dev, &trig, hts221_drdy_handler),
		   "Failed to set HTS221 DRDY trigger");

	int ret = k_sem_take(&hts221_drdy_sem, K_MSEC(2000));

	/* Deregister before asserting so no callback fires after the test exits */
	sensor_trigger_set(dev, &trig, NULL);

	zassert_equal(ret, 0, "HTS221 DRDY interrupt did not fire within 2s");
	LOG_INF("HTS221 DRDY trigger fired");
}
