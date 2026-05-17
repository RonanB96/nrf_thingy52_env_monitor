/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "mock_drivers.h"

DEFINE_FAKE_VALUE_FUNC(int, hts221_driver_init, const struct device *);
DEFINE_FAKE_VALUE_FUNC(int, hts221_driver_power_on);
DEFINE_FAKE_VALUE_FUNC(int, hts221_driver_power_off);
DEFINE_FAKE_VALUE_FUNC(bool, hts221_driver_is_enabled);
DEFINE_FAKE_VALUE_FUNC(int, hts221_driver_read_temperature, float *);
DEFINE_FAKE_VALUE_FUNC(int, hts221_driver_read_humidity, float *);
DEFINE_FAKE_VALUE_FUNC(int, hts221_driver_read_both, float *, float *);

DEFINE_FAKE_VALUE_FUNC(int, lps22hb_driver_init, const struct device *,
		       const struct gpio_dt_spec *);
DEFINE_FAKE_VALUE_FUNC(int, lps22hb_driver_power_on);
DEFINE_FAKE_VALUE_FUNC(int, lps22hb_driver_trigger_oneshot);
DEFINE_FAKE_VALUE_FUNC(int, lps22hb_driver_wait_data_ready, k_timeout_t);
DEFINE_FAKE_VALUE_FUNC(int, lps22hb_driver_power_off);
DEFINE_FAKE_VALUE_FUNC(bool, lps22hb_driver_is_enabled);
DEFINE_FAKE_VALUE_FUNC(int, lps22hb_driver_read_pressure, float *);

DEFINE_FAKE_VALUE_FUNC(int, ccs811_driver_init, const struct device *);
DEFINE_FAKE_VALUE_FUNC(bool, ccs811_driver_is_ready);
DEFINE_FAKE_VALUE_FUNC(int64_t, ccs811_driver_conditioning_time_remaining);
DEFINE_FAKE_VALUE_FUNC(int, ccs811_driver_save_baseline);
DEFINE_FAKE_VALUE_FUNC(int, ccs811_driver_restore_baseline);
DEFINE_FAKE_VALUE_FUNC(bool, ccs811_driver_baseline_save_due);
DEFINE_FAKE_VALUE_FUNC(int, ccs811_driver_run_diagnostics);
DEFINE_FAKE_VALUE_FUNC(int, ccs811_driver_manual_reset);
DEFINE_FAKE_VALUE_FUNC(bool, ccs811_driver_is_enabled);
DEFINE_FAKE_VALUE_FUNC(int, ccs811_driver_read_air_quality, uint16_t *, uint16_t *, float, float);
DEFINE_FAKE_VALUE_FUNC(int, ccs811_driver_read_for_ble, uint16_t *, uint16_t *, float, float);
DEFINE_FAKE_VALUE_FUNC(int, ccs811_driver_get_mode);
DEFINE_FAKE_VALUE_FUNC(int, ccs811_driver_set_mode, enum ccs811_measurement_mode);
