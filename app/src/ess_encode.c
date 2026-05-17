/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Pure encoders for BLE Environmental Sensing Service. See ess_encode.h.
 *
 * Limits:
 *   Temperature/humidity per Bluetooth SIG ESS characteristic YAMLs.
 *   Pressure per LPS22HB datasheet operating range (26.0..126.0 kPa).
 */

#include <math.h>
#include "ess_encode.h"

#define ESS_TEMP_SCALE       100.0f  /* \u00b0C  -> 0.01 \u00b0C */
#define ESS_HUM_SCALE        100.0f  /* %    -> 0.01 % */
#define ESS_PRESS_KPA_TO_DPA 10000.0f /* kPa -> 0.1 Pa */

int16_t ess_encode_temperature(float temp_celsius)
{
	if (isnan(temp_celsius) || isinf(temp_celsius)) {
		return ESS_UNKNOWN_S16;
	}
	float scaled = temp_celsius * ESS_TEMP_SCALE;
	if (scaled <= (float)ESS_TEMP_MIN_E_2C) {
		return (int16_t)ESS_TEMP_MIN_E_2C;
	}
	if (scaled >= (float)ESS_TEMP_MAX_E_2C) {
		return (int16_t)ESS_TEMP_MAX_E_2C;
	}
	return (int16_t)scaled;
}

uint16_t ess_encode_humidity(float humidity_percent)
{
	if (isnan(humidity_percent) || isinf(humidity_percent)) {
		return ESS_UNKNOWN_U16;
	}
	float scaled = humidity_percent * ESS_HUM_SCALE;
	if (scaled <= (float)ESS_HUM_MIN_E_2PCT) {
		return (uint16_t)ESS_HUM_MIN_E_2PCT;
	}
	if (scaled >= (float)ESS_HUM_MAX_E_2PCT) {
		return (uint16_t)ESS_HUM_MAX_E_2PCT;
	}
	return (uint16_t)scaled;
}

uint32_t ess_encode_pressure(float pressure_kpa)
{
	if (isnan(pressure_kpa) || isinf(pressure_kpa)) {
		return (uint32_t)ESS_PRESS_MIN_DPA;
	}
	float scaled = pressure_kpa * ESS_PRESS_KPA_TO_DPA;
	if (scaled <= (float)ESS_PRESS_MIN_DPA) {
		return (uint32_t)ESS_PRESS_MIN_DPA;
	}
	if (scaled >= (float)ESS_PRESS_MAX_DPA) {
		return (uint32_t)ESS_PRESS_MAX_DPA;
	}
	return (uint32_t)scaled;
}

uint16_t ess_encode_co2(uint16_t co2_ppm)
{
	return co2_ppm;
}

uint16_t ess_encode_tvoc(uint16_t tvoc_ppb)
{
	return tvoc_ppb;
}

bool ess_should_notify_16(enum ess_trigger_condition condition, int16_t old_val, int16_t new_val,
			  int16_t ref_val)
{
	switch (condition) {
	case ESS_TRIGGER_INACTIVE:
		return false;
	case ESS_TRIGGER_VALUE_CHANGED:
		return new_val != old_val;
	case ESS_TRIGGER_LESS_THAN_REF_VALUE:
		return new_val < ref_val;
	case ESS_TRIGGER_LESS_OR_EQUAL_TO_REF_VALUE:
		return new_val <= ref_val;
	case ESS_TRIGGER_GREATER_THAN_REF_VALUE:
		return new_val > ref_val;
	case ESS_TRIGGER_GREATER_OR_EQUAL_TO_REF_VALUE:
		return new_val >= ref_val;
	case ESS_TRIGGER_EQUAL_TO_REF_VALUE:
		return new_val == ref_val;
	case ESS_TRIGGER_NOT_EQUAL_TO_REF_VALUE:
		return new_val != ref_val;
	default:
		return false;
	}
}

bool ess_should_notify_32(enum ess_trigger_condition condition, int32_t old_val, int32_t new_val,
			  int32_t ref_val)
{
	switch (condition) {
	case ESS_TRIGGER_INACTIVE:
		return false;
	case ESS_TRIGGER_VALUE_CHANGED:
		return new_val != old_val;
	case ESS_TRIGGER_LESS_THAN_REF_VALUE:
		return new_val < ref_val;
	case ESS_TRIGGER_LESS_OR_EQUAL_TO_REF_VALUE:
		return new_val <= ref_val;
	case ESS_TRIGGER_GREATER_THAN_REF_VALUE:
		return new_val > ref_val;
	case ESS_TRIGGER_GREATER_OR_EQUAL_TO_REF_VALUE:
		return new_val >= ref_val;
	case ESS_TRIGGER_EQUAL_TO_REF_VALUE:
		return new_val == ref_val;
	case ESS_TRIGGER_NOT_EQUAL_TO_REF_VALUE:
		return new_val != ref_val;
	default:
		return false;
	}
}
