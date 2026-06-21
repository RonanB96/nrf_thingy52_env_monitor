/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Pure encoding helpers for the BLE Environmental Sensing Service.
 * Separated from ess_service.c so they can be unit-tested without the BT
 * stack. No Zephyr/BT dependencies.
 *
 * Wire formats per Bluetooth SIG ESS YAMLs in docs/datasheets/bluetooth-sig/:
 *   temperature  : sint16, 0.01 \u00b0C   ; "value not known" = 0x8000
 *   humidity     : uint16, 0.01 %      ; "value not known" = 0xFFFF
 *   pressure     : uint32, 0.1 Pa      ; no fixed range
 *   CO2 / VOC    : uint16, ppm / ppb   ; "value not known" = 0xFFFF
 */

#ifndef ESS_ENCODE_H_
#define ESS_ENCODE_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* BT SIG "value not known" sentinels. */
#define ESS_UNKNOWN_S16 ((int16_t)0x8000)
#define ESS_UNKNOWN_U16 0xFFFFU

/* Wire-format limits (see ess_encode.c for citations). */
#define ESS_TEMP_MIN_E_2C  (-27315)
#define ESS_TEMP_MAX_E_2C  32767
#define ESS_HUM_MIN_E_2PCT 0U
#define ESS_HUM_MAX_E_2PCT 10000U
#define ESS_PRESS_MIN_DPA  260000UL  /* 260.0 hPa */
#define ESS_PRESS_MAX_DPA  1260000UL /* 1260.0 hPa */

/* ESS Trigger Setting conditions (Bluetooth SIG ESS, Trigger Setting descriptor). */
enum ess_trigger_condition {
	ESS_TRIGGER_INACTIVE = 0x00,
	ESS_TRIGGER_FIXED_TIME_INTERVAL = 0x01,
	ESS_TRIGGER_NO_LESS_THAN_SPECIFIED_TIME = 0x02,
	ESS_TRIGGER_VALUE_CHANGED = 0x03,
	ESS_TRIGGER_LESS_THAN_REF_VALUE = 0x04,
	ESS_TRIGGER_LESS_OR_EQUAL_TO_REF_VALUE = 0x05,
	ESS_TRIGGER_GREATER_THAN_REF_VALUE = 0x06,
	ESS_TRIGGER_GREATER_OR_EQUAL_TO_REF_VALUE = 0x07,
	ESS_TRIGGER_EQUAL_TO_REF_VALUE = 0x08,
	ESS_TRIGGER_NOT_EQUAL_TO_REF_VALUE = 0x09,
};

/*
 * Encode floating-point sensor reading into BLE wire format. Out-of-range
 * inputs are clamped to the valid range; NaN/Inf return the SIG "unknown"
 * sentinel where one is defined.
 */
int16_t ess_encode_temperature(float temp_celsius);
uint16_t ess_encode_humidity(float humidity_percent);
uint32_t ess_encode_pressure(float pressure_kpa);
uint16_t ess_encode_co2(uint16_t co2_ppm);
uint16_t ess_encode_tvoc(uint16_t tvoc_ppb);

/*
 * Trigger evaluation for ESS notifications. Returns true when the trigger
 * condition is satisfied for the given old/new/reference values.
 */
bool ess_should_notify_16(enum ess_trigger_condition condition, int16_t old_val, int16_t new_val,
			  int16_t ref_val);
bool ess_should_notify_32(enum ess_trigger_condition condition, int32_t old_val, int32_t new_val,
			  int32_t ref_val);

#ifdef __cplusplus
}
#endif

#endif /* ESS_ENCODE_H_ */
