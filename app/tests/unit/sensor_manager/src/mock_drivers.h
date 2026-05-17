/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * FFF fakes for the three thin sensor driver wrappers used by
 * app/src/sensor_manager.c. These let us drive every code path of
 * sensor_manager (success, failure, conditioning, range-clamp) without
 * any real I2C transactions.
 */

#ifndef MOCK_DRIVERS_H_
#define MOCK_DRIVERS_H_

#include <zephyr/fff.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>

#include "sensor_hts221_driver.h"
#include "sensor_lps22hb_driver.h"
#include "sensor_ccs811_driver.h"

/* HTS221 */
DECLARE_FAKE_VALUE_FUNC(int, hts221_driver_init, const struct device *);
DECLARE_FAKE_VALUE_FUNC(int, hts221_driver_power_on);
DECLARE_FAKE_VALUE_FUNC(int, hts221_driver_power_off);
DECLARE_FAKE_VALUE_FUNC(bool, hts221_driver_is_enabled);
DECLARE_FAKE_VALUE_FUNC(int, hts221_driver_read_temperature, float *);
DECLARE_FAKE_VALUE_FUNC(int, hts221_driver_read_humidity, float *);
DECLARE_FAKE_VALUE_FUNC(int, hts221_driver_read_both, float *, float *);

/* LPS22HB */
DECLARE_FAKE_VALUE_FUNC(int, lps22hb_driver_init, const struct device *,
			const struct gpio_dt_spec *);
DECLARE_FAKE_VALUE_FUNC(int, lps22hb_driver_power_on);
DECLARE_FAKE_VALUE_FUNC(int, lps22hb_driver_trigger_oneshot);
DECLARE_FAKE_VALUE_FUNC(int, lps22hb_driver_wait_data_ready, k_timeout_t);
DECLARE_FAKE_VALUE_FUNC(int, lps22hb_driver_power_off);
DECLARE_FAKE_VALUE_FUNC(bool, lps22hb_driver_is_enabled);
DECLARE_FAKE_VALUE_FUNC(int, lps22hb_driver_read_pressure, float *);

/* CCS811 */
DECLARE_FAKE_VALUE_FUNC(int, ccs811_driver_init, const struct device *);
DECLARE_FAKE_VALUE_FUNC(bool, ccs811_driver_is_ready);
DECLARE_FAKE_VALUE_FUNC(int64_t, ccs811_driver_conditioning_time_remaining);
DECLARE_FAKE_VALUE_FUNC(int, ccs811_driver_save_baseline);
DECLARE_FAKE_VALUE_FUNC(int, ccs811_driver_restore_baseline);
DECLARE_FAKE_VALUE_FUNC(bool, ccs811_driver_baseline_save_due);
DECLARE_FAKE_VALUE_FUNC(int, ccs811_driver_run_diagnostics);
DECLARE_FAKE_VALUE_FUNC(int, ccs811_driver_manual_reset);
DECLARE_FAKE_VALUE_FUNC(bool, ccs811_driver_is_enabled);
DECLARE_FAKE_VALUE_FUNC(int, ccs811_driver_read_air_quality, uint16_t *, uint16_t *, float, float);
DECLARE_FAKE_VALUE_FUNC(int, ccs811_driver_read_for_ble, uint16_t *, uint16_t *, float, float);
DECLARE_FAKE_VALUE_FUNC(int, ccs811_driver_get_mode);
DECLARE_FAKE_VALUE_FUNC(int, ccs811_driver_set_mode, enum ccs811_measurement_mode);

#define MOCK_DRIVERS_FFF_FAKES_LIST(FAKE)                                                          \
	FAKE(hts221_driver_init)                                                                   \
	FAKE(hts221_driver_power_on)                                                               \
	FAKE(hts221_driver_power_off)                                                              \
	FAKE(hts221_driver_is_enabled)                                                             \
	FAKE(hts221_driver_read_temperature)                                                       \
	FAKE(hts221_driver_read_humidity)                                                          \
	FAKE(hts221_driver_read_both)                                                              \
	FAKE(lps22hb_driver_init)                                                                  \
	FAKE(lps22hb_driver_power_on)                                                              \
	FAKE(lps22hb_driver_trigger_oneshot)                                                       \
	FAKE(lps22hb_driver_wait_data_ready)                                                       \
	FAKE(lps22hb_driver_power_off)                                                             \
	FAKE(lps22hb_driver_is_enabled)                                                            \
	FAKE(lps22hb_driver_read_pressure)                                                         \
	FAKE(ccs811_driver_init)                                                                   \
	FAKE(ccs811_driver_is_ready)                                                               \
	FAKE(ccs811_driver_conditioning_time_remaining)                                            \
	FAKE(ccs811_driver_save_baseline)                                                          \
	FAKE(ccs811_driver_restore_baseline)                                                       \
	FAKE(ccs811_driver_baseline_save_due)                                                      \
	FAKE(ccs811_driver_run_diagnostics)                                                        \
	FAKE(ccs811_driver_manual_reset)                                                           \
	FAKE(ccs811_driver_is_enabled)                                                             \
	FAKE(ccs811_driver_read_air_quality)                                                       \
	FAKE(ccs811_driver_read_for_ble)                                                           \
	FAKE(ccs811_driver_get_mode)                                                               \
	FAKE(ccs811_driver_set_mode)

#endif /* MOCK_DRIVERS_H_ */
