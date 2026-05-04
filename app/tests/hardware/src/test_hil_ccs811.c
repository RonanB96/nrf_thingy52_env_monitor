/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(test_hil_ccs811, LOG_LEVEL_INF);

ZTEST_SUITE(hil_ccs811, NULL, NULL, NULL, NULL, NULL);

ZTEST(hil_ccs811, test_i2c_present)
{
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ccs811));

	/*
	 * CCS811 requires a 20-minute conditioning period before producing
	 * valid CO2/TVOC readings. sensor_sample_fetch() returns -EAGAIN
	 * when data is not yet ready — this is expected and proves the chip
	 * is present and ACKing on I2C. Any other negative return code
	 * indicates a hardware or bus failure.
	 */
	zassert_true(device_is_ready(dev), "CCS811 not ready");

	int rc = sensor_sample_fetch(dev);

	zassert_true(rc == 0 || rc == -EAGAIN,
		     "CCS811 I2C fetch failed (rc=%d) — chip not responding", rc);
	LOG_INF("CCS811 fetch rc=%d (%s)", rc, rc == 0 ? "data ready" : "conditioning");
}
