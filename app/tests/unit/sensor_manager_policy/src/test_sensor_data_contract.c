/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Tests for the SENSOR_DATA_IS_VALID macro defined in sensor_manager.h.
 * That macro is MY code (header-defined preprocessor logic) and is the
 * only behavioural unit in the public sensor_manager contract that can
 * be exercised without instantiating the full sensor pipeline.
 *
 * Tests for header constants (e.g. SENSOR_ENV_BASIC == TEMP|HUM|PRES|BAT)
 * and Kconfig defaults (e.g. CONFIG_SENSOR_ENV_INTERVAL_SEC == 900) were
 * removed: they compare a symbol to its own definition (tautology) or
 * verify a default in Kconfig (configuration, not code). Neither tests
 * a unit of MY code.
 *
 * The behavioural sensor_manager logic lives in
 * app/tests/unit/sensor_manager/.
 */

#include <zephyr/ztest.h>

#include "sensor_manager.h"

ZTEST_SUITE(sensor_data_contract, NULL, NULL, NULL, NULL, NULL);

ZTEST(sensor_data_contract, test_macro_returns_false_for_clear_bit)
{
	struct sensor_data d = {0};

	zassert_false(SENSOR_DATA_IS_VALID(&d, SENSOR_TEMPERATURE),
		      "macro must report invalid when bit is clear");
	zassert_false(SENSOR_DATA_IS_VALID(&d, SENSOR_HUMIDITY));
	zassert_false(SENSOR_DATA_IS_VALID(&d, SENSOR_PRESSURE));
	zassert_false(SENSOR_DATA_IS_VALID(&d, SENSOR_AIR_QUALITY));
	zassert_false(SENSOR_DATA_IS_VALID(&d, SENSOR_BATTERY));
}

ZTEST(sensor_data_contract, test_macro_returns_true_for_set_bit_only)
{
	struct sensor_data d = {0};

	d.valid_mask |= SENSOR_TEMPERATURE;

	zassert_true(SENSOR_DATA_IS_VALID(&d, SENSOR_TEMPERATURE),
		     "macro must report valid when target bit is set");
	zassert_false(SENSOR_DATA_IS_VALID(&d, SENSOR_HUMIDITY),
		      "macro must NOT report valid for an unset, unrelated bit");
}

ZTEST(sensor_data_contract, test_macro_isolates_to_queried_bit)
{
	/* Setting one flag must not be reported as another flag being valid:
	 * verifies the macro masks with the argument, not the whole word. */
	struct sensor_data d = {0};

	d.valid_mask |= SENSOR_AIR_QUALITY;

	zassert_true(SENSOR_DATA_IS_VALID(&d, SENSOR_AIR_QUALITY));
	zassert_false(SENSOR_DATA_IS_VALID(&d, SENSOR_BATTERY));
	zassert_false(SENSOR_DATA_IS_VALID(&d, SENSOR_TEMPERATURE));
}
