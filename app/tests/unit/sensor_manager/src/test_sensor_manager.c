/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Unit tests for app/src/sensor_manager.c.
 *
 * The unit-under-test holds static state (initialized, armed, current_data,
 * connected_count, registered callback) that cannot be reset between tests.
 * Pre-init guards therefore cannot run after the first test that calls init,
 * so they live in their own ZTEST_SUITE binary if isolation is needed; here
 * we exercise them by name "test_pre_*" and rely on the fact that they only
 * read state. Behaviour tests reset all FFF fakes and force the callback to
 * NULL in before_each so each test starts from a clean observation surface.
 *
 * Strategy: for every test, drive a known input via FFF custom_fake or
 * return_val, exercise MY code, then assert MY observable output (cache,
 * getters, valid_mask, on-the-wire fan-out parameters). We never assert
 * "Zephyr did the right thing" -- only that my code produced the contracted
 * output for the given input.
 */

#include <zephyr/ztest.h>
#include <zephyr/fff.h>
#include <zephyr/kernel.h>
#include <math.h>
#include <string.h>

#include "sensor_manager.h"
#include "mock_drivers.h"
#include "mock_battery_service.h"

/* DEFINE_FFF_GLOBALS lives in the shared mocks library (fff_globals.c). */

/* ---- Callback observation ------------------------------------------------ */

static int callback_call_count;
static struct sensor_data last_callback_data;

static void test_callback(const struct sensor_data *data)
{
	callback_call_count++;
	last_callback_data = *data;
}

/* ---- CCS811 read helpers (drive known inputs into MY code) -------------- */

/* Captures the temperature and humidity that MY code passes to the CCS811
 * driver as environmental compensation arguments. */
static float captured_t;
static float captured_h;

static int read_aq_capture_compensation(uint16_t *co2, uint16_t *tvoc, float t, float h)
{
	captured_t = t;
	captured_h = h;
	if (co2) {
		*co2 = 600U;
	}
	if (tvoc) {
		*tvoc = 25U;
	}
	return 0;
}

static int read_aq_valid(uint16_t *co2, uint16_t *tvoc, float t, float h)
{
	ARG_UNUSED(t);
	ARG_UNUSED(h);
	if (co2) {
		*co2 = 600U;
	}
	if (tvoc) {
		*tvoc = 25U;
	}
	return 0;
}

static int read_aq_ble_valid(uint16_t *co2, uint16_t *tvoc, float t, float h)
{
	ARG_UNUSED(t);
	ARG_UNUSED(h);
	if (co2) {
		*co2 = 750U;
	}
	if (tvoc) {
		*tvoc = 40U;
	}
	return 0;
}

static int read_aq_invalid_range(uint16_t *co2, uint16_t *tvoc, float t, float h)
{
	ARG_UNUSED(t);
	ARG_UNUSED(h);
	if (co2) {
		*co2 = CCS811_INVALID_READING; /* sentinel */
	}
	if (tvoc) {
		*tvoc = 5000U; /* > CCS811_TVOC_MAX_PPB (1187) */
	}
	return 0;
}

/* eCO2 in-range, TVOC out-of-range: exercises the asymmetric branch in
 * read_air_quality(): individual cached values updated independently, but
 * SENSOR_AIR_QUALITY flag only set when BOTH are valid. */
static int read_aq_only_eco2_valid(uint16_t *co2, uint16_t *tvoc, float t, float h)
{
	ARG_UNUSED(t);
	ARG_UNUSED(h);
	if (co2) {
		*co2 = 700U;
	}
	if (tvoc) {
		*tvoc = 9999U; /* out of range */
	}
	return 0;
}

static int read_aq_only_tvoc_valid(uint16_t *co2, uint16_t *tvoc, float t, float h)
{
	ARG_UNUSED(t);
	ARG_UNUSED(h);
	if (co2) {
		*co2 = 0U; /* invalid: 0 fails the eco2 != 0 check */
	}
	if (tvoc) {
		*tvoc = 100U;
	}
	return 0;
}

/* ---- HTS221 / LPS22HB read helpers -------------------------------------- */

static int read_both_valid(float *temp, float *hum)
{
	if (temp) {
		*temp = 21.5f;
	}
	if (hum) {
		*hum = 47.0f;
	}
	return 0;
}

static int read_pressure_valid(float *p)
{
	if (p) {
		*p = 1013.25f;
	}
	return 0;
}

/* ---- Suite plumbing ----------------------------------------------------- */

static void reset_all_fakes(void)
{
	MOCK_DRIVERS_FFF_FAKES_LIST(RESET_FAKE);
	BATTERY_SERVICE_FFF_FAKES_LIST(RESET_FAKE);
	FFF_RESET_HISTORY();
	callback_call_count = 0;
	memset(&last_callback_data, 0, sizeof(last_callback_data));
	captured_t = 0.0f;
	captured_h = 0.0f;
}

static void before_each(void *fixture)
{
	ARG_UNUSED(fixture);
	reset_all_fakes();
	/* Detach any callback a previous test registered so update() does not
	 * inadvertently invoke stale callbacks. The unit-under-test allows NULL. */
	(void)sensor_manager_register_callback(NULL);
}

ZTEST_SUITE(sensor_manager, NULL, NULL, before_each, NULL, NULL);

/* ---- Pre-init guards ---------------------------------------------------- */
/* These tests rely on running before any test in this binary calls init().
 * ztest does not contractually guarantee ordering; we use the "test_pre_*"
 * prefix so they sort first under the default lex ordering. If a future
 * change breaks that, move them to a separate suite binary. */

ZTEST(sensor_manager, test_pre_a_get_data_pre_init_returns_einval)
{
	struct sensor_data d;
	zassert_equal(sensor_manager_get_data(&d), -EINVAL);
}

ZTEST(sensor_manager, test_pre_b_update_pre_init_returns_einval)
{
	zassert_equal(sensor_manager_update(), -EINVAL);
}

ZTEST(sensor_manager, test_pre_c_update_selective_pre_init_returns_einval)
{
	zassert_equal(sensor_manager_update_selective(SENSOR_ENV_BASIC), -EINVAL);
}

ZTEST(sensor_manager, test_pre_d_arm_pre_init_returns_einval)
{
	zassert_equal(sensor_manager_arm(), -EINVAL);
}

ZTEST(sensor_manager, test_pre_e_on_connected_unarmed_returns_einval)
{
	zassert_equal(sensor_manager_on_connected(), -EINVAL);
}

ZTEST(sensor_manager, test_pre_f_pre_init_getters_return_zero)
{
	zassert_equal(sensor_manager_get_temperature(), 0.0f);
	zassert_equal(sensor_manager_get_humidity(), 0.0f);
	zassert_equal(sensor_manager_get_pressure(), 0.0f);
	zassert_equal(sensor_manager_get_eco2(), 0U);
	zassert_equal(sensor_manager_get_tvoc(), 0U);
	zassert_equal(sensor_manager_get_battery_level(), 0U);
}

ZTEST(sensor_manager, test_pre_g_on_disconnected_no_count_is_safe)
{
	/* MY contract: warn and return without crashing when count == 0. */
	sensor_manager_on_disconnected();
}

ZTEST(sensor_manager, test_pre_h_is_ccs811_ready_proxies_to_driver)
{
	ccs811_driver_is_ready_fake.return_val = true;
	zassert_true(sensor_manager_is_ccs811_ready());
	zassert_equal(ccs811_driver_is_ready_fake.call_count, 1);

	ccs811_driver_is_ready_fake.return_val = false;
	zassert_false(sensor_manager_is_ccs811_ready());
}

/* ---- Init --------------------------------------------------------------- */

ZTEST(sensor_manager, test_q01_init_succeeds_and_invokes_each_driver_init)
{
	hts221_driver_read_both_fake.custom_fake = read_both_valid;
	lps22hb_driver_read_pressure_fake.custom_fake = read_pressure_valid;
	ccs811_driver_read_air_quality_fake.custom_fake = read_aq_valid;
	battery_service_get_level_fake.return_val = 88;
	battery_service_is_charging_fake.return_val = false;

	int ret = sensor_manager_init();
	zassert_equal(ret, 0, "init returned %d", ret);

	zassert_equal(battery_service_init_fake.call_count, 1,
		      "init must call battery_service_init exactly once");
	zassert_equal(hts221_driver_init_fake.call_count, 1);
	zassert_equal(lps22hb_driver_init_fake.call_count, 1);
	zassert_equal(ccs811_driver_init_fake.call_count, 1);
}

ZTEST(sensor_manager, test_q02_init_is_idempotent)
{
	int ret = sensor_manager_init();
	zassert_equal(ret, 0);
	/* Second call must short-circuit and not re-init drivers. */
	zassert_equal(hts221_driver_init_fake.call_count, 0,
		      "idempotent init must not re-init HTS221");
	zassert_equal(lps22hb_driver_init_fake.call_count, 0);
	zassert_equal(ccs811_driver_init_fake.call_count, 0);
	zassert_equal(battery_service_init_fake.call_count, 0);
}

/* ---- Update fan-out and caching ---------------------------------------- */

ZTEST(sensor_manager, test_r01_update_writes_full_cache_and_invokes_callback)
{
	hts221_driver_read_both_fake.custom_fake = read_both_valid;
	lps22hb_driver_read_pressure_fake.custom_fake = read_pressure_valid;
	ccs811_driver_read_air_quality_fake.custom_fake = read_aq_valid;
	battery_service_get_level_fake.return_val = 75;
	battery_service_is_charging_fake.return_val = true;

	zassert_equal(sensor_manager_register_callback(test_callback), 0);
	callback_call_count = 0;

	zassert_equal(sensor_manager_update(), 0);

	/* Each driver read called exactly once. */
	zassert_equal(hts221_driver_read_both_fake.call_count, 1);
	zassert_equal(lps22hb_driver_read_pressure_fake.call_count, 1);
	zassert_equal(ccs811_driver_read_air_quality_fake.call_count, 1);
	zassert_equal(battery_service_get_level_fake.call_count, 1);

	/* Cache reflects driver outputs. */
	zassert_within(sensor_manager_get_temperature(), 21.5f, 0.01f);
	zassert_within(sensor_manager_get_humidity(), 47.0f, 0.01f);
	zassert_within(sensor_manager_get_pressure(), 1013.25f, 0.01f);
	zassert_equal(sensor_manager_get_eco2(), 600U);
	zassert_equal(sensor_manager_get_tvoc(), 25U);
	zassert_equal(sensor_manager_get_battery_level(), 75U);

	/* Callback fired exactly once with the same payload as the cache. */
	zassert_equal(callback_call_count, 1);
	zassert_true((last_callback_data.valid_mask & SENSOR_TEMPERATURE) != 0);
	zassert_true((last_callback_data.valid_mask & SENSOR_HUMIDITY) != 0);
	zassert_true((last_callback_data.valid_mask & SENSOR_PRESSURE) != 0);
	zassert_true((last_callback_data.valid_mask & SENSOR_AIR_QUALITY) != 0);
	zassert_true((last_callback_data.valid_mask & SENSOR_BATTERY) != 0);
	zassert_true(last_callback_data.battery_charging);
}

ZTEST(sensor_manager, test_r02_update_air_quality_invalid_range_clears_flag)
{
	ccs811_driver_read_air_quality_fake.custom_fake = read_aq_invalid_range;

	zassert_equal(sensor_manager_update_selective(SENSOR_AIR_QUALITY), 0);

	struct sensor_data snap;
	zassert_equal(sensor_manager_get_data(&snap), 0);
	zassert_equal(snap.valid_mask & SENSOR_AIR_QUALITY, 0,
		      "out-of-range readings must clear SENSOR_AIR_QUALITY in valid_mask");
	zassert_equal(sensor_manager_get_eco2(), 0U);
	zassert_equal(sensor_manager_get_tvoc(), 0U);
}

ZTEST(sensor_manager, test_r03_air_quality_only_eco2_valid_does_not_set_flag)
{
	/* Asymmetric branch in read_air_quality(): when only one of the two
	 * is in range, the cached value is updated but SENSOR_AIR_QUALITY is
	 * NOT set (the public mask requires both). */
	ccs811_driver_read_air_quality_fake.custom_fake = read_aq_only_eco2_valid;

	zassert_equal(sensor_manager_update_selective(SENSOR_AIR_QUALITY), 0);

	struct sensor_data snap;
	zassert_equal(sensor_manager_get_data(&snap), 0);
	zassert_equal(snap.valid_mask & SENSOR_AIR_QUALITY, 0,
		      "asymmetric validity must NOT set SENSOR_AIR_QUALITY");
	/* Getters honour the mask, so they read 0 even though the cache field
	 * for eco2 was internally written. That's the public contract. */
	zassert_equal(sensor_manager_get_eco2(), 0U);
}

ZTEST(sensor_manager, test_r04_air_quality_only_tvoc_valid_does_not_set_flag)
{
	ccs811_driver_read_air_quality_fake.custom_fake = read_aq_only_tvoc_valid;

	zassert_equal(sensor_manager_update_selective(SENSOR_AIR_QUALITY), 0);

	struct sensor_data snap;
	zassert_equal(sensor_manager_get_data(&snap), 0);
	zassert_equal(snap.valid_mask & SENSOR_AIR_QUALITY, 0,
		      "asymmetric validity must NOT set SENSOR_AIR_QUALITY");
}

ZTEST(sensor_manager, test_r05_update_selective_only_reads_selected)
{
	hts221_driver_read_both_fake.custom_fake = read_both_valid;
	lps22hb_driver_read_pressure_fake.custom_fake = read_pressure_valid;

	zassert_equal(sensor_manager_update_selective(SENSOR_TEMP_HUMIDITY), 0);

	zassert_equal(hts221_driver_read_both_fake.call_count, 1);
	zassert_equal(lps22hb_driver_read_pressure_fake.call_count, 0,
		      "pressure must NOT be read when not selected");
	zassert_equal(ccs811_driver_read_air_quality_fake.call_count, 0);
	zassert_equal(battery_service_get_level_fake.call_count, 0);
}

ZTEST(sensor_manager, test_r06_get_data_null_pointer_returns_einval)
{
	zassert_equal(sensor_manager_get_data(NULL), -EINVAL);
}

/* ---- Per-driver failure must surface in valid_mask --------------------- */

ZTEST(sensor_manager, test_r07_hts221_failure_clears_temp_humidity_only)
{
	/* Seed cache with valid values for everything. */
	hts221_driver_read_both_fake.custom_fake = read_both_valid;
	lps22hb_driver_read_pressure_fake.custom_fake = read_pressure_valid;
	ccs811_driver_read_air_quality_fake.custom_fake = read_aq_valid;
	battery_service_get_level_fake.return_val = 50;
	zassert_equal(sensor_manager_update(), 0);

	struct sensor_data before;
	zassert_equal(sensor_manager_get_data(&before), 0);
	zassert_true((before.valid_mask & SENSOR_TEMPERATURE) != 0);
	zassert_true((before.valid_mask & SENSOR_PRESSURE) != 0);

	/* Now make HTS221 fail on the next update. */
	hts221_driver_read_both_fake.custom_fake = NULL;
	hts221_driver_read_both_fake.return_val = -EIO;

	zassert_equal(sensor_manager_update(), 0);

	struct sensor_data after;
	zassert_equal(sensor_manager_get_data(&after), 0);
	zassert_equal(after.valid_mask & SENSOR_TEMPERATURE, 0,
		      "HTS221 failure must clear SENSOR_TEMPERATURE");
	zassert_equal(after.valid_mask & SENSOR_HUMIDITY, 0,
		      "HTS221 failure must clear SENSOR_HUMIDITY");
	zassert_true((after.valid_mask & SENSOR_PRESSURE) != 0,
		     "HTS221 failure must NOT touch SENSOR_PRESSURE");
	zassert_true((after.valid_mask & SENSOR_AIR_QUALITY) != 0,
		     "HTS221 failure must NOT touch SENSOR_AIR_QUALITY");
}

ZTEST(sensor_manager, test_r08_lps22hb_failure_clears_pressure_only)
{
	hts221_driver_read_both_fake.custom_fake = read_both_valid;
	lps22hb_driver_read_pressure_fake.custom_fake = read_pressure_valid;
	ccs811_driver_read_air_quality_fake.custom_fake = read_aq_valid;
	battery_service_get_level_fake.return_val = 50;
	zassert_equal(sensor_manager_update(), 0);

	lps22hb_driver_read_pressure_fake.custom_fake = NULL;
	lps22hb_driver_read_pressure_fake.return_val = -EIO;

	zassert_equal(sensor_manager_update(), 0);

	struct sensor_data after;
	zassert_equal(sensor_manager_get_data(&after), 0);
	zassert_equal(after.valid_mask & SENSOR_PRESSURE, 0,
		      "LPS22HB failure must clear SENSOR_PRESSURE");
	zassert_true((after.valid_mask & SENSOR_TEMPERATURE) != 0,
		     "LPS22HB failure must NOT touch SENSOR_TEMPERATURE");
}

/* ---- Environmental compensation pass-through to CCS811 ----------------- */

ZTEST(sensor_manager, test_r09_compensation_passes_cached_temp_humidity_to_ccs811)
{
	hts221_driver_read_both_fake.custom_fake = read_both_valid;
	lps22hb_driver_read_pressure_fake.custom_fake = read_pressure_valid;
	ccs811_driver_read_air_quality_fake.custom_fake = read_aq_capture_compensation;
	battery_service_get_level_fake.return_val = 50;

	zassert_equal(sensor_manager_update(), 0);

	zassert_within(captured_t, 21.5f, 0.01f,
		       "CCS811 compensation must receive cached temperature, got %f",
		       (double)captured_t);
	zassert_within(captured_h, 47.0f, 0.01f,
		       "CCS811 compensation must receive cached humidity, got %f",
		       (double)captured_h);
}

ZTEST(sensor_manager, test_r10_compensation_passes_nan_when_temp_humidity_invalid)
{
	/* Make HTS221 fail so SENSOR_TEMPERATURE/HUMIDITY bits are cleared. */
	hts221_driver_read_both_fake.return_val = -EIO;
	lps22hb_driver_read_pressure_fake.custom_fake = read_pressure_valid;
	ccs811_driver_read_air_quality_fake.custom_fake = read_aq_capture_compensation;
	battery_service_get_level_fake.return_val = 50;

	zassert_equal(sensor_manager_update(), 0);

	zassert_true(isnan(captured_t),
		     "CCS811 compensation temp must be NaN when SENSOR_TEMPERATURE invalid");
	zassert_true(isnan(captured_h),
		     "CCS811 compensation humidity must be NaN when SENSOR_HUMIDITY invalid");
}

/* ---- Arm + connection lifecycle --------------------------------------- */

ZTEST(sensor_manager, test_s01_arm_requires_callback)
{
	/* before_each forced callback NULL. */
	zassert_equal(sensor_manager_arm(), -EINVAL,
		      "arm must reject when no callback registered");
}

ZTEST(sensor_manager, test_s02_arm_succeeds_with_callback)
{
	zassert_equal(sensor_manager_register_callback(test_callback), 0);
	zassert_equal(sensor_manager_arm(), 0);
}

ZTEST(sensor_manager, test_s03_on_connected_after_arm_does_env_then_aq_update)
{
	zassert_equal(sensor_manager_register_callback(test_callback), 0);
	zassert_equal(sensor_manager_arm(), 0);

	hts221_driver_read_both_fake.custom_fake = read_both_valid;
	lps22hb_driver_read_pressure_fake.custom_fake = read_pressure_valid;
	ccs811_driver_read_air_quality_fake.custom_fake = read_aq_valid;
	battery_service_get_level_fake.return_val = 70;

	/* Reset counters to isolate effect of on_connected. */
	hts221_driver_read_both_fake.call_count = 0;
	lps22hb_driver_read_pressure_fake.call_count = 0;
	ccs811_driver_read_air_quality_fake.call_count = 0;
	battery_service_get_level_fake.call_count = 0;

	zassert_equal(sensor_manager_on_connected(), 0);

	/* MY contract per sensor_manager.c on_connected():
	 *   update_selective(SENSOR_ENV_BASIC) -> hts221+lps22hb+battery x1
	 *   update_selective(SENSOR_AIR_QUALITY) -> ccs811 x1
	 */
	zassert_equal(hts221_driver_read_both_fake.call_count, 1,
		      "on_connected must trigger one env update");
	zassert_equal(lps22hb_driver_read_pressure_fake.call_count, 1);
	zassert_equal(battery_service_get_level_fake.call_count, 1);
	zassert_equal(ccs811_driver_read_air_quality_fake.call_count, 1,
		      "on_connected must trigger one AQ update");

	/* Bring the connection counter back to zero so subsequent tests start
	 * from a clean lifecycle state. */
	sensor_manager_on_disconnected();
}

/* ---- BLE-optimized AQ update ------------------------------------------ */

ZTEST(sensor_manager, test_t01_update_air_quality_for_ble_caches_valid)
{
	ccs811_driver_read_for_ble_fake.custom_fake = read_aq_ble_valid;

	sensor_manager_update_air_quality_for_ble();

	zassert_equal(ccs811_driver_read_for_ble_fake.call_count, 1);

	struct sensor_data snap;
	zassert_equal(sensor_manager_get_data(&snap), 0);
	zassert_true((snap.valid_mask & SENSOR_AIR_QUALITY) != 0,
		     "valid AQ read must set SENSOR_AIR_QUALITY");
	zassert_equal(snap.eco2, 750U);
	zassert_equal(snap.tvoc, 40U);
	zassert_equal(sensor_manager_get_eco2(), 750U);
	zassert_equal(sensor_manager_get_tvoc(), 40U);
}

ZTEST(sensor_manager, test_t02_update_air_quality_for_ble_keeps_cache_on_failure)
{
	/* Seed cache. */
	ccs811_driver_read_for_ble_fake.custom_fake = read_aq_ble_valid;
	sensor_manager_update_air_quality_for_ble();

	struct sensor_data before;
	zassert_equal(sensor_manager_get_data(&before), 0);
	zassert_true((before.valid_mask & SENSOR_AIR_QUALITY) != 0);

	/* Drive failure. */
	ccs811_driver_read_for_ble_fake.custom_fake = NULL;
	ccs811_driver_read_for_ble_fake.return_val = -EAGAIN;

	sensor_manager_update_air_quality_for_ble();

	struct sensor_data after;
	zassert_equal(sensor_manager_get_data(&after), 0);
	zassert_true((after.valid_mask & SENSOR_AIR_QUALITY) != 0,
		     "BLE-AQ failure must NOT clear cached SENSOR_AIR_QUALITY");
	zassert_equal(after.eco2, before.eco2,
		      "BLE-AQ failure must NOT alter cached eco2");
	zassert_equal(after.tvoc, before.tvoc);
}
