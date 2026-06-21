/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Tests for app/src/ess_encode.c. No mocks needed — pure functions.
 */

#include <math.h>
#include <stdint.h>
#include <zephyr/ztest.h>
#include "ess_encode.h"

ZTEST_SUITE(ess_encode, NULL, NULL, NULL, NULL, NULL);

/* ---- temperature: sint16, 0.01 \u00b0C, allowed [-27315..32767], NaN -> 0x8000 ---- */

ZTEST(ess_encode, test_temperature_nominal_room)
{
	zassert_equal(ess_encode_temperature(20.00f), 2000);
	zassert_equal(ess_encode_temperature(-10.00f), -1000);
	zassert_equal(ess_encode_temperature(0.00f), 0);
}

ZTEST(ess_encode, test_temperature_clamps_low_at_absolute_zero)
{
	zassert_equal(ess_encode_temperature(-300.0f), ESS_TEMP_MIN_E_2C);
	zassert_equal(ess_encode_temperature(-273.15f), ESS_TEMP_MIN_E_2C);
}

ZTEST(ess_encode, test_temperature_clamps_high_at_sint16_max)
{
	zassert_equal(ess_encode_temperature(400.0f), ESS_TEMP_MAX_E_2C);
	zassert_equal(ess_encode_temperature(327.67f), ESS_TEMP_MAX_E_2C);
}

ZTEST(ess_encode, test_temperature_nan_returns_unknown_sentinel)
{
	zassert_equal(ess_encode_temperature(NAN), ESS_UNKNOWN_S16);
	zassert_equal(ess_encode_temperature(INFINITY), ESS_UNKNOWN_S16);
	zassert_equal(ess_encode_temperature(-INFINITY), ESS_UNKNOWN_S16);
}

/* ---- humidity: uint16, 0.01 %, allowed [0..10000], NaN -> 0xFFFF ---- */

ZTEST(ess_encode, test_humidity_nominal)
{
	zassert_equal(ess_encode_humidity(50.00f), 5000);
	zassert_equal(ess_encode_humidity(0.00f), 0);
	zassert_equal(ess_encode_humidity(100.00f), 10000);
}

ZTEST(ess_encode, test_humidity_clamps_negative_to_zero)
{
	zassert_equal(ess_encode_humidity(-5.0f), 0);
}

ZTEST(ess_encode, test_humidity_clamps_above_100pct)
{
	zassert_equal(ess_encode_humidity(150.0f), 10000);
}

ZTEST(ess_encode, test_humidity_nan_returns_unknown_sentinel)
{
	zassert_equal(ess_encode_humidity(NAN), ESS_UNKNOWN_U16);
	zassert_equal(ess_encode_humidity(INFINITY), ESS_UNKNOWN_U16);
}

/* ---- pressure: uint32, 0.1 Pa, clamped to LPS22HB range ---- */

ZTEST(ess_encode, test_pressure_atmospheric_round_trips)
{
	/* 101.325 kPa -> 1013250 (0.1 Pa). Allow ±1 unit for float rounding. */
	uint32_t v = ess_encode_pressure(101.325f);

	zassert_within(v, 1013250UL, 1UL, "expected ~1013250, got %u", v);
}

ZTEST(ess_encode, test_pressure_clamps_below_lps22hb_minimum)
{
	zassert_equal(ess_encode_pressure(10.0f), (uint32_t)ESS_PRESS_MIN_DPA);
	zassert_equal(ess_encode_pressure(-5.0f), (uint32_t)ESS_PRESS_MIN_DPA);
}

ZTEST(ess_encode, test_pressure_clamps_above_lps22hb_maximum)
{
	/* 200.0 kPa -> 2_000_000 > MAX (1_260_000) -> clamps */
	zassert_equal(ess_encode_pressure(200.0f), (uint32_t)ESS_PRESS_MAX_DPA);
}

ZTEST(ess_encode, test_pressure_nan_or_inf_returns_min)
{
	/* NaN/Inf are treated as "no usable reading" and clamped to the lower
	 * sensor limit, matching ess_encode_pressure() implementation. */
	zassert_equal(ess_encode_pressure(NAN), (uint32_t)ESS_PRESS_MIN_DPA);
	zassert_equal(ess_encode_pressure(INFINITY), (uint32_t)ESS_PRESS_MIN_DPA);
	zassert_equal(ess_encode_pressure(-INFINITY), (uint32_t)ESS_PRESS_MIN_DPA);
}

/* ---- co2 / tvoc: passthrough ---- */

ZTEST(ess_encode, test_co2_passthrough)
{
	zassert_equal(ess_encode_co2(0), 0);
	zassert_equal(ess_encode_co2(400), 400);
	zassert_equal(ess_encode_co2(0xFFFF), 0xFFFF);
}

ZTEST(ess_encode, test_tvoc_passthrough)
{
	zassert_equal(ess_encode_tvoc(0), 0);
	zassert_equal(ess_encode_tvoc(150), 150);
	zassert_equal(ess_encode_tvoc(0xFFFF), 0xFFFF);
}

/* ---- trigger evaluation: 16-bit ---- */

ZTEST(ess_encode, test_notify16_inactive_never_fires)
{
	zassert_false(ess_should_notify_16(ESS_TRIGGER_INACTIVE, 0, 0, 0));
	zassert_false(ess_should_notify_16(ESS_TRIGGER_INACTIVE, 100, 200, 50));
}

ZTEST(ess_encode, test_notify16_value_changed)
{
	zassert_true(ess_should_notify_16(ESS_TRIGGER_VALUE_CHANGED, 100, 101, 0));
	zassert_false(ess_should_notify_16(ESS_TRIGGER_VALUE_CHANGED, 100, 100, 0));
}

ZTEST(ess_encode, test_notify16_lt_le_gt_ge)
{
	zassert_true(ess_should_notify_16(ESS_TRIGGER_LESS_THAN_REF_VALUE, 0, 9, 10));
	zassert_false(ess_should_notify_16(ESS_TRIGGER_LESS_THAN_REF_VALUE, 0, 10, 10));

	zassert_true(ess_should_notify_16(ESS_TRIGGER_LESS_OR_EQUAL_TO_REF_VALUE, 0, 10, 10));
	zassert_false(ess_should_notify_16(ESS_TRIGGER_LESS_OR_EQUAL_TO_REF_VALUE, 0, 11, 10));

	zassert_true(ess_should_notify_16(ESS_TRIGGER_GREATER_THAN_REF_VALUE, 0, 11, 10));
	zassert_false(ess_should_notify_16(ESS_TRIGGER_GREATER_THAN_REF_VALUE, 0, 10, 10));

	zassert_true(ess_should_notify_16(ESS_TRIGGER_GREATER_OR_EQUAL_TO_REF_VALUE, 0, 10, 10));
	zassert_false(ess_should_notify_16(ESS_TRIGGER_GREATER_OR_EQUAL_TO_REF_VALUE, 0, 9, 10));
}

ZTEST(ess_encode, test_notify16_eq_ne)
{
	zassert_true(ess_should_notify_16(ESS_TRIGGER_EQUAL_TO_REF_VALUE, 0, 10, 10));
	zassert_false(ess_should_notify_16(ESS_TRIGGER_EQUAL_TO_REF_VALUE, 0, 9, 10));

	zassert_true(ess_should_notify_16(ESS_TRIGGER_NOT_EQUAL_TO_REF_VALUE, 0, 9, 10));
	zassert_false(ess_should_notify_16(ESS_TRIGGER_NOT_EQUAL_TO_REF_VALUE, 0, 10, 10));
}

ZTEST(ess_encode, test_notify16_unknown_condition_is_false)
{
	/* Conditions 0x01 (FIXED_TIME_INTERVAL) and 0x02 are not handled
	 * by the encoder switch; they must default to false. */
	zassert_false(ess_should_notify_16(ESS_TRIGGER_FIXED_TIME_INTERVAL, 0, 1, 0));
	zassert_false(ess_should_notify_16(ESS_TRIGGER_NO_LESS_THAN_SPECIFIED_TIME, 0, 1, 0));
}

/* ---- trigger evaluation: 32-bit (mirror of 16-bit, exercises wider integers) ---- */

ZTEST(ess_encode, test_notify32_value_changed_and_compares)
{
	zassert_true(ess_should_notify_32(ESS_TRIGGER_VALUE_CHANGED, 100000, 100001, 0));
	zassert_false(ess_should_notify_32(ESS_TRIGGER_VALUE_CHANGED, 100000, 100000, 0));

	zassert_true(ess_should_notify_32(ESS_TRIGGER_LESS_THAN_REF_VALUE, 0, 999999, 1000000));
	zassert_false(ess_should_notify_32(ESS_TRIGGER_LESS_THAN_REF_VALUE, 0, 1000000, 1000000));

	zassert_true(ess_should_notify_32(ESS_TRIGGER_GREATER_OR_EQUAL_TO_REF_VALUE, 0, 1000000,
					  1000000));
	zassert_false(ess_should_notify_32(ESS_TRIGGER_GREATER_OR_EQUAL_TO_REF_VALUE, 0, 999999,
					   1000000));
}

ZTEST(ess_encode, test_notify32_inactive_and_eq_ne)
{
	zassert_false(ess_should_notify_32(ESS_TRIGGER_INACTIVE, 0, 0, 0));
	zassert_true(ess_should_notify_32(ESS_TRIGGER_EQUAL_TO_REF_VALUE, 0, 5, 5));
	zassert_true(ess_should_notify_32(ESS_TRIGGER_NOT_EQUAL_TO_REF_VALUE, 0, 4, 5));
}

ZTEST(ess_encode, test_notify32_le_gt_ne_branches)
{
	/* Cover the remaining 32-bit comparison branches and the false sides
	 * not exercised by the wider test above. */
	zassert_true(ess_should_notify_32(ESS_TRIGGER_LESS_OR_EQUAL_TO_REF_VALUE, 0, 1000000,
					  1000000));
	zassert_false(ess_should_notify_32(ESS_TRIGGER_LESS_OR_EQUAL_TO_REF_VALUE, 0, 1000001,
					   1000000));

	zassert_true(ess_should_notify_32(ESS_TRIGGER_GREATER_THAN_REF_VALUE, 0, 1000001,
					  1000000));
	zassert_false(ess_should_notify_32(ESS_TRIGGER_GREATER_THAN_REF_VALUE, 0, 1000000,
					   1000000));

	zassert_false(ess_should_notify_32(ESS_TRIGGER_EQUAL_TO_REF_VALUE, 0, 4, 5));
	zassert_false(ess_should_notify_32(ESS_TRIGGER_NOT_EQUAL_TO_REF_VALUE, 0, 5, 5));
}

ZTEST(ess_encode, test_notify32_unknown_condition_is_false)
{
	/* Mirror the 16-bit "default" coverage: any condition not handled by
	 * the encoder switch must default to false. */
	zassert_false(ess_should_notify_32(ESS_TRIGGER_FIXED_TIME_INTERVAL, 0, 1, 0));
	zassert_false(ess_should_notify_32(ESS_TRIGGER_NO_LESS_THAN_SPECIFIED_TIME, 0, 1, 0));
}
