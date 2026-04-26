/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>
#include "sensor_manager.h"

LOG_MODULE_REGISTER(test_sensor_manager_policy, CONFIG_LOG_DEFAULT_LEVEL);

#define TEST_AQ_DIVISOR_TWO 2U

#ifdef CONFIG_SENSOR_ENV_INTERVAL_SEC
#define TEST_SAMPLING_INTERVAL_SEC CONFIG_SENSOR_ENV_INTERVAL_SEC
#else
#define TEST_SAMPLING_INTERVAL_SEC 900U
#endif

/* CCS811 validation constants — must match sensor_manager.c definitions */
#define TEST_CCS811_INVALID_READING 65021U /* 0xFE0D */
#define TEST_CCS811_ECO2_MIN_PPM    400U
#define TEST_CCS811_ECO2_MAX_PPM    8192U
#define TEST_CCS811_TVOC_MIN_PPB    0U
#define TEST_CCS811_TVOC_MAX_PPB    1187U

/* -------------------------------------------------------------------------
 * Helpers
 * -----------------------------------------------------------------------*/

/**
 * Replicates the AQ divisor decision from sensor_work_handler.
 * Returns true when the cycle should include an air-quality read.
 */
static inline bool should_read_aq(uint32_t cycle, uint32_t divisor)
{
	return (cycle % divisor) == 0;
}

/**
 * Replicates the eCO2 validation logic from sensor_manager.c.
 * Returns true when the reading is within the accepted hardware range.
 */
static inline bool eco2_reading_valid(uint16_t eco2)
{
	return eco2 != TEST_CCS811_INVALID_READING && eco2 != 0U &&
	       eco2 >= TEST_CCS811_ECO2_MIN_PPM && eco2 <= TEST_CCS811_ECO2_MAX_PPM;
}

/**
 * Replicates the TVOC validation logic from sensor_manager.c.
 * Returns true when the reading is within the accepted hardware range.
 */
static inline bool tvoc_reading_valid(uint16_t tvoc)
{
	return tvoc != TEST_CCS811_INVALID_READING && tvoc >= TEST_CCS811_TVOC_MIN_PPB &&
	       tvoc <= TEST_CCS811_TVOC_MAX_PPB;
}

/* -------------------------------------------------------------------------
 * ZTEST Suite: sensor_manager_aq_divisor
 * Tests AQ sub-sampling policy in periodic sampling.
 * sensor_work_handler increments aq_cycle before each periodic update and
 * only includes SENSOR_AIR_QUALITY when (aq_cycle % divisor == 0).
 * -----------------------------------------------------------------------*/

ZTEST_SUITE(sensor_manager_aq_divisor, NULL, NULL, NULL, NULL, NULL);

ZTEST(sensor_manager_aq_divisor, test_first_cycle_skips_aq)
{
	/* Cycle 1 (first scheduled read): odd → AQ should be skipped */
	zassert_false(should_read_aq(1, TEST_AQ_DIVISOR_TWO),
		      "Cycle 1 must NOT read AQ (divisor=%u)", TEST_AQ_DIVISOR_TWO);
}

ZTEST(sensor_manager_aq_divisor, test_second_cycle_reads_aq)
{
	/* Cycle 2: even → AQ should be included */
	zassert_true(should_read_aq(2, TEST_AQ_DIVISOR_TWO), "Cycle 2 must read AQ (divisor=%u)",
		     TEST_AQ_DIVISOR_TWO);
}

ZTEST(sensor_manager_aq_divisor, test_odd_cycles_skip_aq)
{
	/* All odd cycles should skip AQ */
	for (uint32_t cycle = 1; cycle <= 9; cycle += 2) {
		zassert_false(should_read_aq(cycle, TEST_AQ_DIVISOR_TWO),
			      "Odd cycle %u must NOT read AQ", cycle);
	}
}

ZTEST(sensor_manager_aq_divisor, test_even_cycles_read_aq)
{
	/* All even cycles should include AQ */
	for (uint32_t cycle = 2; cycle <= 10; cycle += 2) {
		zassert_true(should_read_aq(cycle, TEST_AQ_DIVISOR_TWO), "Even cycle %u must read AQ",
			     cycle);
	}
}

ZTEST(sensor_manager_aq_divisor, test_flags_with_aq)
{
	/* When AQ is included, the full sensor flag set must contain SENSOR_AIR_QUALITY */
	uint32_t full_flags = SENSOR_TEMPERATURE | SENSOR_HUMIDITY | SENSOR_PRESSURE |
			      SENSOR_BATTERY | SENSOR_AIR_QUALITY;

	zassert_not_equal(full_flags & SENSOR_AIR_QUALITY, 0U,
			  "Full flag set must include SENSOR_AIR_QUALITY");
}

ZTEST(sensor_manager_aq_divisor, test_flags_without_aq)
{
	/* When AQ is skipped, the flag set must NOT contain SENSOR_AIR_QUALITY */
	uint32_t skip_aq_flags =
		SENSOR_TEMPERATURE | SENSOR_HUMIDITY | SENSOR_PRESSURE | SENSOR_BATTERY;

	zassert_equal(skip_aq_flags & SENSOR_AIR_QUALITY, 0U,
		      "Skip-AQ flag set must NOT include SENSOR_AIR_QUALITY");
}

ZTEST(sensor_manager_aq_divisor, test_divisor_one_always_reads_aq)
{
	/* With divisor=1, every cycle should read AQ */
	for (uint32_t cycle = 1; cycle <= 5; cycle++) {
		zassert_true(should_read_aq(cycle, 1),
			     "With divisor=1, cycle %u must always read AQ", cycle);
	}
}

/* -------------------------------------------------------------------------
 * ZTEST Suite: sensor_manager_ccs811_validation
 * Tests CCS811 reading validation logic.
 * sensor_manager.c filters out-of-range and sentinel-value readings.
 * -----------------------------------------------------------------------*/

ZTEST_SUITE(sensor_manager_ccs811_validation, NULL, NULL, NULL, NULL, NULL);

ZTEST(sensor_manager_ccs811_validation, test_eco2_valid_range)
{
	/* Minimum valid eCO2: 400 ppm (atmospheric CO2 baseline) */
	zassert_true(eco2_reading_valid(TEST_CCS811_ECO2_MIN_PPM),
		     "Minimum eCO2 %u ppm must be valid", TEST_CCS811_ECO2_MIN_PPM);

	/* Mid-range reading */
	zassert_true(eco2_reading_valid(1000U), "Mid-range eCO2 1000 ppm must be valid");

	/* Maximum valid eCO2 per CCS811 spec */
	zassert_true(eco2_reading_valid(TEST_CCS811_ECO2_MAX_PPM),
		     "Maximum eCO2 %u ppm must be valid", TEST_CCS811_ECO2_MAX_PPM);
}

ZTEST(sensor_manager_ccs811_validation, test_eco2_invalid_sentinel)
{
	/* 0xFE0D is the hardware sentinel for "not ready" */
	zassert_false(eco2_reading_valid(TEST_CCS811_INVALID_READING),
		      "Sentinel 0x%04X must be invalid", TEST_CCS811_INVALID_READING);
}

ZTEST(sensor_manager_ccs811_validation, test_eco2_invalid_zero)
{
	/* Zero is not a valid eCO2 reading */
	zassert_false(eco2_reading_valid(0U), "Zero eCO2 must be invalid");
}

ZTEST(sensor_manager_ccs811_validation, test_eco2_below_minimum)
{
	/* Below atmospheric baseline is not a valid reading */
	zassert_false(eco2_reading_valid(TEST_CCS811_ECO2_MIN_PPM - 1U),
		      "eCO2 below minimum (%u ppm) must be invalid", TEST_CCS811_ECO2_MIN_PPM - 1U);
}

ZTEST(sensor_manager_ccs811_validation, test_eco2_above_maximum)
{
	/* Above hardware maximum is not a valid reading */
	zassert_false(eco2_reading_valid(TEST_CCS811_ECO2_MAX_PPM + 1U),
		      "eCO2 above maximum (%u ppm) must be invalid", TEST_CCS811_ECO2_MAX_PPM + 1U);
}

ZTEST(sensor_manager_ccs811_validation, test_tvoc_valid_range)
{
	/* Zero TVOC is valid (clean air) */
	zassert_true(tvoc_reading_valid(TEST_CCS811_TVOC_MIN_PPB),
		     "TVOC 0 ppb must be valid (clean air baseline)");

	/* Mid-range TVOC */
	zassert_true(tvoc_reading_valid(500U), "TVOC 500 ppb must be valid");

	/* Maximum valid TVOC per CCS811 spec */
	zassert_true(tvoc_reading_valid(TEST_CCS811_TVOC_MAX_PPB),
		     "Maximum TVOC %u ppb must be valid", TEST_CCS811_TVOC_MAX_PPB);
}

ZTEST(sensor_manager_ccs811_validation, test_tvoc_invalid_sentinel)
{
	zassert_false(tvoc_reading_valid(TEST_CCS811_INVALID_READING),
		      "Sentinel 0x%04X must be invalid for TVOC", TEST_CCS811_INVALID_READING);
}

ZTEST(sensor_manager_ccs811_validation, test_tvoc_above_maximum)
{
	zassert_false(tvoc_reading_valid(TEST_CCS811_TVOC_MAX_PPB + 1U),
		      "TVOC above maximum (%u ppb) must be invalid", TEST_CCS811_TVOC_MAX_PPB + 1U);
}

/* -------------------------------------------------------------------------
 * ZTEST Suite: sensor_manager_sensor_data
 * Tests the sensor_data structure contract.
 * Zero-initialised structs must have all validity flags false.
 * -----------------------------------------------------------------------*/

ZTEST_SUITE(sensor_manager_sensor_data, NULL, NULL, NULL, NULL, NULL);

ZTEST(sensor_manager_sensor_data, test_zero_init_all_invalid)
{
	struct sensor_data d = {0};

	zassert_false(d.temperature_valid, "Zero-init temperature_valid must be false");
	zassert_false(d.humidity_valid, "Zero-init humidity_valid must be false");
	zassert_false(d.pressure_valid, "Zero-init pressure_valid must be false");
	zassert_false(d.eco2_valid, "Zero-init eco2_valid must be false");
	zassert_false(d.tvoc_valid, "Zero-init tvoc_valid must be false");
	zassert_false(d.battery_valid, "Zero-init battery_valid must be false");
}

ZTEST(sensor_manager_sensor_data, test_validity_flags_set_independently)
{
	struct sensor_data d = {0};

	d.temperature = 22.5f;
	d.temperature_valid = true;

	zassert_true(d.temperature_valid, "temperature_valid should be true after set");
	zassert_false(d.humidity_valid, "humidity_valid must remain false");
	zassert_false(d.pressure_valid, "pressure_valid must remain false");
	zassert_false(d.eco2_valid, "eco2_valid must remain false");
	zassert_false(d.tvoc_valid, "tvoc_valid must remain false");
}

ZTEST(sensor_manager_sensor_data, test_sampling_interval_is_900s)
{
	/* Guard against accidental reduction of the periodic sampling interval. */
	zassert_equal(TEST_SAMPLING_INTERVAL_SEC, 900U,
		      "Sampling interval must be 900 s (15 min) for battery life target");
}
