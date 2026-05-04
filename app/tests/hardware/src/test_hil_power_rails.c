/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(test_hil_power_rails, LOG_LEVEL_INF);

ZTEST_SUITE(hil_power_rails, NULL, NULL, NULL, NULL, NULL);

/*
 * Power rail verification: MPU and microphone power must be off.
 * The app initialises SX1509B with init-out-low for pins 8 (MPU_PWR_CTRL)
 * and 9 (MIC_PWR_CTRL) and nothing in the test suite enables them.
 * gpio_pin_get_raw reads the SX1509B DATA register directly from hardware.
 */
ZTEST(hil_power_rails, test_sx1509b_mpu_mic_off)
{
	const struct device *sx = DEVICE_DT_GET(DT_NODELABEL(sx1509b));

	zassert_true(device_is_ready(sx), "SX1509B not ready");

	int mpu = gpio_pin_get_raw(sx, 8);
	int mic = gpio_pin_get_raw(sx, 9);

	LOG_INF("SX1509B pin8 (MPU_PWR_CTRL)=%d  pin9 (MIC_PWR_CTRL)=%d", mpu, mic);
	zassert_true(mpu >= 0, "MPU_PWR_CTRL (pin 8) GPIO read failed (err=%d)", mpu);
	zassert_equal(mpu, 0, "MPU_PWR_CTRL (pin 8) should be LOW — device must stay powered off");
	zassert_true(mic >= 0, "MIC_PWR_CTRL (pin 9) GPIO read failed (err=%d)", mic);
	zassert_equal(mic, 0, "MIC_PWR_CTRL (pin 9) should be LOW — device must stay powered off");
}
