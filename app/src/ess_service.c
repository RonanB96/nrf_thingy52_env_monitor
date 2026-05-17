/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <zephyr/bluetooth/att.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include "ess_encode.h"
#include "ess_service.h"
#include "sensor_manager.h"

LOG_MODULE_REGISTER(ess_service, CONFIG_LOG_DEFAULT_LEVEL);

/* ESS Measurement Descriptor – Sampling Functions - Bluetooth SIG standard */
enum ess_sampling_func {
	ESS_SAMPLING_UNSPECIFIED = 0x00,
	ESS_SAMPLING_INSTANTANEOUS = 0x01,
	ESS_SAMPLING_ARITHMETIC_MEAN = 0x02,
	ESS_SAMPLING_RMS = 0x03,
	ESS_SAMPLING_MAXIMUM = 0x04,
	ESS_SAMPLING_MINIMUM = 0x05,
	ESS_SAMPLING_ACCUMULATED = 0x06,
	ESS_SAMPLING_COUNT = 0x07,
};

/* ES Measurement Descriptor - Applications - Bluetooth SIG standard */
enum ess_application {
	ESS_APP_UNSPECIFIED = 0x00,
	ESS_APP_AIR = 0x01,
	ESS_APP_OUTDOOR = 0x13,
	ESS_APP_INDOOR = 0x14,
};

_Static_assert((int)ESS_TRIGGER_NOT_EQUAL_TO_REF_VALUE <= UINT8_MAX,
	       "ess_trigger_condition must fit in uint8_t for BLE wire format");
_Static_assert((int)ESS_SAMPLING_COUNT <= UINT8_MAX,
	       "ess_sampling_func must fit in uint8_t for BLE wire format");
_Static_assert((int)ESS_APP_INDOOR <= UINT8_MAX,
	       "ess_application must fit in uint8_t for BLE wire format");

/* Sensor valid ranges (wire-format). Authoritative copies live in ess_encode.h. */
#define TEMP_LOWER_LIMIT     ESS_TEMP_MIN_E_2C
#define TEMP_UPPER_LIMIT     ESS_TEMP_MAX_E_2C
#define HUMIDITY_LOWER_LIMIT ESS_HUM_MIN_E_2PCT
#define HUMIDITY_UPPER_LIMIT ESS_HUM_MAX_E_2PCT
#define PRESSURE_LOWER_LIMIT ESS_PRESS_MIN_DPA
#define PRESSURE_UPPER_LIMIT ESS_PRESS_MAX_DPA
#define CO2_LOWER_LIMIT      400U   /* 400 ppm (CCS811 floor) */
#define CO2_UPPER_LIMIT      5000U  /* 5000 ppm (CCS811 ceiling) */
#define TVOC_LOWER_LIMIT     0U     /* 0 ppb */
#define TVOC_UPPER_LIMIT     32767U /* CCS811 advertised range */

/* Environmental Sensing Service measurements - Bluetooth SIG ESS Standard */
struct es_measurement {
	uint16_t flags; /* Reserved for Future Use - always 0x0000 per SIG spec */
	enum ess_sampling_func sampling_func; /* Sampling Function field:
					       * ESS_SAMPLING_UNSPECIFIED    = 0x00
					       * ESS_SAMPLING_INSTANTANEOUS  = 0x01
					       * ESS_SAMPLING_ARITHMETIC_MEAN = 0x02
					       * ESS_SAMPLING_RMS            = 0x03
					       * ESS_SAMPLING_MAXIMUM        = 0x04
					       * ESS_SAMPLING_MINIMUM        = 0x05
					       * ESS_SAMPLING_ACCUMULATED    = 0x06
					       * ESS_SAMPLING_COUNT          = 0x07 */
	uint32_t meas_period; /* Measurement Period - time in seconds over which sampling occurs:
			       * 0x000000 = Not applicable (for instantaneous measurements)
			       * 0x000001-0xFFFFFF = Period in seconds (1 sec to ~194 days) */
	uint32_t update_interval;         /* Update Interval - time in seconds between measurements:
					   * 0x000000 = Not applicable (for on-demand/event-driven updates)
					   * 0x000001-0xFFFFFF = Interval in seconds (1 sec to ~194 days) */
	enum ess_application application; /* Application field:
					   * ESS_APP_UNSPECIFIED = 0x00
					   * ESS_APP_AIR         = 0x01
					   * ESS_APP_OUTDOOR     = 0x13
					   * ESS_APP_INDOOR      = 0x14 */
	uint8_t meas_uncertainty; /* Measurement Uncertainty - accuracy/precision information:
				   * Expressed in same units as the measurement
				   * Represents ± uncertainty range (e.g., ±0.5°C = 0x32 for temp)
				   */
};

/* Sensor data structures */
struct ess_sensor {
	int32_t value;       /* Current sensor value - stored in characteristic's native units:
			      * Temperature: 0.01°C resolution (2000 = 20.00°C)
			      * Humidity: 0.01% resolution (5000 = 50.00%)
			      * Pressure: 0.1 Pa resolution (101325 = 1013.25 hPa)
			      * CO2: 1 ppm resolution (400 = 400 ppm)
			      * TVOC: 1 ppb resolution (0 = 0 ppb) */
	int32_t lower_limit; /* Valid Range Lower Limit - minimum acceptable value */
	int32_t upper_limit; /* Valid Range Upper Limit - maximum acceptable value */
	enum ess_trigger_condition condition; /* Trigger Setting Condition:
					       * ESS_TRIGGER_INACTIVE
					       * ESS_TRIGGER_VALUE_CHANGED
					       * ESS_TRIGGER_LESS_THAN_REF_VALUE
					       * ESS_TRIGGER_GREATER_THAN_REF_VALUE
					       * etc. (see enum ess_trigger_condition) */
	int32_t ref_val; /* Trigger Setting Reference Value - comparison value for conditional
			    triggers */
	struct es_measurement
		meas;        /* ES Measurement Descriptor - describes how measurement is obtained */
	bool notify_enabled; /* Client Characteristic Configuration - true if client enabled
				notifications */
	bool value_known;    /* True once the cached value came from a validated sensor reading. */
};

/* ESS sensors state */

/* Measurement uncertainty values (used in static initializers, must be #define or enum -- not
 * static const -- for file-scope struct initialization in C). */
#define ESS_TEMP_UNCERTAINTY      0x32U  /* 50 x 0.01°C = ±0.50°C */
#define ESS_HUM_UNCERTAINTY       0x32U  /* 50 x 0.01% = ±0.50%RH */
#define ESS_PRESS_UNCERTAINTY     0x96U  /* 150 x 0.1 Pa = ±15.0 Pa */
#define ESS_CO2_UNCERTAINTY       0x64U  /* 100 ppm */
#define ESS_TVOC_UNCERTAINTY      0x32U  /* 50 ppb */

#define ESS_PRESS_MIN_KPA 26.0f
#define ESS_PRESS_MAX_KPA 126.0f

static struct ess_sensor temperature_sensor = {
	.value = ESS_UNKNOWN_S16,
	.lower_limit = TEMP_LOWER_LIMIT,
	.upper_limit = TEMP_UPPER_LIMIT,
	.condition = ESS_TRIGGER_VALUE_CHANGED,
	.ref_val = 0,
	.meas =
		{
			.flags = 0,
			.sampling_func = ESS_SAMPLING_INSTANTANEOUS,
			.meas_period = 0x00,     /* On-demand readings */
			.update_interval = 0x00, /* No fixed interval - read on demand */
			.application = ESS_APP_INDOOR,
			.meas_uncertainty = ESS_TEMP_UNCERTAINTY, /* ±0.50°C in 0.01°C units (50) */
		},
	.notify_enabled = false,
	.value_known = false,
};

static struct ess_sensor humidity_sensor = {
	.value = ESS_UNKNOWN_U16,
	.lower_limit = HUMIDITY_LOWER_LIMIT,
	.upper_limit = HUMIDITY_UPPER_LIMIT,
	.condition = ESS_TRIGGER_VALUE_CHANGED,
	.ref_val = 0,
	.meas =
		{
			.flags = 0,
			.sampling_func = ESS_SAMPLING_INSTANTANEOUS,
			.meas_period = 0x00,     /* On-demand readings */
			.update_interval = 0x00, /* No fixed interval - read on demand */
			.application = ESS_APP_INDOOR,
			.meas_uncertainty = ESS_HUM_UNCERTAINTY, /* ±0.50%RH in 0.01% units (50) */
		},
	.notify_enabled = false,
	.value_known = false,
};

static struct ess_sensor pressure_sensor = {
	.value = 0,
	.lower_limit = PRESSURE_LOWER_LIMIT,
	.upper_limit = PRESSURE_UPPER_LIMIT,
	.condition = ESS_TRIGGER_VALUE_CHANGED,
	.ref_val = 0,
	.meas =
		{
			.flags = 0,
			.sampling_func = ESS_SAMPLING_INSTANTANEOUS,
			.meas_period = 0x00,     /* On-demand readings */
			.update_interval = 0x00, /* No fixed interval - read on demand */
			.application = ESS_APP_INDOOR,
			.meas_uncertainty =
				ESS_PRESS_UNCERTAINTY, /* ±15.0 Pa in 0.1 Pa units (150
							* 0.1 Pa = 15.0 Pa = 0.15 hPa) */
		},
	.notify_enabled = false,
	.value_known = false,
};

static struct ess_sensor co2_sensor = {
	.value = ESS_UNKNOWN_U16, /* 0xFFFF - value not known (BT SIG) until CCS811 warmed up */
	.lower_limit = CO2_LOWER_LIMIT,
	.upper_limit = CO2_UPPER_LIMIT,
	.condition = ESS_TRIGGER_VALUE_CHANGED,
	.ref_val = 0,
	.meas =
		{
			.flags = 0,
			.sampling_func = ESS_SAMPLING_INSTANTANEOUS,
			.meas_period = 0x00,     /* On-demand readings - no averaging period */
			.update_interval = 0x00, /* No fixed interval - read on demand */
			.application = ESS_APP_AIR,
			.meas_uncertainty = ESS_CO2_UNCERTAINTY, /* ±100 ppm (typical for CCS811) */
		},
	.notify_enabled = false,
	.value_known = false,
};

static struct ess_sensor tvoc_sensor = {
	.value = ESS_UNKNOWN_U16, /* 0xFFFF - value not known (BT SIG) until CCS811 warmed up */
	.lower_limit = TVOC_LOWER_LIMIT,
	.upper_limit = TVOC_UPPER_LIMIT,
	.condition = ESS_TRIGGER_VALUE_CHANGED,
	.ref_val = 0,
	.meas =
		{
			.flags = 0,
			.sampling_func = ESS_SAMPLING_INSTANTANEOUS,
			.meas_period = 0x00,     /* On-demand readings - no averaging period */
			.update_interval = 0x00, /* No fixed interval - read on demand */
			.application = ESS_APP_AIR,
			.meas_uncertainty =
				ESS_TVOC_UNCERTAINTY, /* ±50 ppb (reasonable for TVOC sensor) */
		},
	.notify_enabled = false,
	.value_known = false,
};

static bool ess_initialized = false;

/* GATT attribute indices for characteristics */
enum ess_char_attr_idx {
	ESS_ATTR_IDX_TEMPERATURE = 1, /* Temperature characteristic */
	ESS_ATTR_IDX_HUMIDITY = 7,    /* Humidity characteristic */
	ESS_ATTR_IDX_PRESSURE = 13,   /* Pressure characteristic */
	ESS_ATTR_IDX_CO2 = 19,        /* CO2 characteristic */
	ESS_ATTR_IDX_TVOC = 25,       /* TVOC characteristic */
};

/* ESS sensor conversion scale factors and conditioning constants */
static const uint32_t ESS_INT16_MASK = 0xFFFFU;          /* Mask for int16_t range */
static const int64_t CCS811_CONDITIONING_MS = 1200000LL; /* 20-minute conditioning */
static const int64_t ESS_MS_PER_SEC = 1000LL;            /* Milliseconds per second */

/* Helper functions */
static void invalidate_sensor(struct ess_sensor *sensor)
{
	sensor->value_known = false;
}

static ssize_t ess_att_error_from_errno(int err)
{
	if (err == -ERANGE) {
		return BT_GATT_ERR(BT_ATT_ERR_OUT_OF_RANGE);
	}

	return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
}

static bool ess_temperature_in_range(float temp_celsius)
{
	return isfinite(temp_celsius) && temp_celsius >= -273.15f && temp_celsius <= 327.67f;
}

static bool ess_humidity_in_range(float humidity_percent)
{
	return isfinite(humidity_percent) && humidity_percent >= 0.0f && humidity_percent <= 100.0f;
}

static bool ess_pressure_in_range(float pressure_kpa)
{
	return isfinite(pressure_kpa) && pressure_kpa >= ESS_PRESS_MIN_KPA &&
	       pressure_kpa <= ESS_PRESS_MAX_KPA;
}

static bool ess_co2_in_range(uint16_t co2_ppm)
{
	return co2_ppm >= CO2_LOWER_LIMIT && co2_ppm <= CO2_UPPER_LIMIT;
}

static bool ess_tvoc_in_range(uint16_t tvoc_ppb)
{
	return tvoc_ppb >= TVOC_LOWER_LIMIT && tvoc_ppb <= TVOC_UPPER_LIMIT;
}

static ssize_t read_u16(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
			uint16_t len, uint16_t offset)
{
	const int32_t *value_ptr = attr->user_data;

	/* Trigger fresh sensor reading based on which characteristic is being read */
	if (value_ptr == &temperature_sensor.value || value_ptr == &humidity_sensor.value) {
		struct sensor_data fresh_data;
		bool reading_temperature = (value_ptr == &temperature_sensor.value);
		bool reading_humidity = (value_ptr == &humidity_sensor.value);
		int ret;

		/* Temperature or Humidity - trigger fresh temp/humidity sensor update */
		ret = sensor_manager_update_selective(SENSOR_TEMP_HUMIDITY);
		if (ret != 0) {
			LOG_WRN("Failed to trigger temp/humidity update: %d", ret);
			invalidate_sensor(reading_temperature ? &temperature_sensor : &humidity_sensor);
			return ess_att_error_from_errno(ret);
		}

		ret = sensor_manager_get_data(&fresh_data);
		if (ret != 0) {
			LOG_WRN("Failed to fetch temp/humidity data: %d", ret);
			invalidate_sensor(reading_temperature ? &temperature_sensor : &humidity_sensor);
			return ess_att_error_from_errno(ret);
		}

		if (SENSOR_DATA_IS_VALID(&fresh_data, SENSOR_TEMPERATURE)) {
			if (!ess_temperature_in_range(fresh_data.temperature)) {
				LOG_ERR("Temperature %.2f out of range", (double)fresh_data.temperature);
				invalidate_sensor(&temperature_sensor);
				if (reading_temperature) {
					return ess_att_error_from_errno(-ERANGE);
				}
			} else {
				temperature_sensor.value = (int32_t)ess_encode_temperature(fresh_data.temperature);
				temperature_sensor.value_known = true;
			}
		} else {
			invalidate_sensor(&temperature_sensor);
			if (reading_temperature) {
				return ess_att_error_from_errno(-ENODATA);
			}
		}

		if (SENSOR_DATA_IS_VALID(&fresh_data, SENSOR_HUMIDITY)) {
			if (!ess_humidity_in_range(fresh_data.humidity)) {
				LOG_ERR("Humidity %.2f out of range", (double)fresh_data.humidity);
				invalidate_sensor(&humidity_sensor);
				if (reading_humidity) {
					return ess_att_error_from_errno(-ERANGE);
				}
			} else {
				humidity_sensor.value = (int32_t)ess_encode_humidity(fresh_data.humidity);
				humidity_sensor.value_known = true;
			}
		} else {
			invalidate_sensor(&humidity_sensor);
			if (reading_humidity) {
				return ess_att_error_from_errno(-ENODATA);
			}
		}

		LOG_DBG("Fresh temp/humidity update: T=%.2f°C, H=%.1f%%",
			(double)fresh_data.temperature, (double)fresh_data.humidity);
	}

	if ((value_ptr == &temperature_sensor.value && !temperature_sensor.value_known) ||
	    (value_ptr == &humidity_sensor.value && !humidity_sensor.value_known)) {
		return ess_att_error_from_errno(-ENODATA);
	}

	/* Ensure value is within int16_t range before conversion */
	int16_t value = (int16_t)(*value_ptr & ESS_INT16_MASK);
	uint16_t le_value = sys_cpu_to_le16((uint16_t)value);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &le_value, sizeof(le_value));
}

static ssize_t read_co2(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
			uint16_t len, uint16_t offset)
{
	if (!sensor_manager_is_ccs811_ready()) {
		/* CCS811 still conditioning - report 0xFFFF (value not known) per BT SIG */
		co2_sensor.value = ESS_UNKNOWN_U16;
		co2_sensor.value_known = true;
	} else {
		struct sensor_data fresh_data;

		sensor_manager_update_air_quality_for_ble();

		int ret = sensor_manager_get_data(&fresh_data);
		if (ret != 0 || !SENSOR_DATA_IS_VALID(&fresh_data, SENSOR_AIR_QUALITY)) {
			LOG_WRN("Failed to fetch valid CO2 data: %d", ret);
			invalidate_sensor(&co2_sensor);
			return ess_att_error_from_errno(ret != 0 ? ret : -ENODATA);
		}

		if (!ess_co2_in_range(fresh_data.eco2)) {
			LOG_ERR("CO2 %u out of range", fresh_data.eco2);
			invalidate_sensor(&co2_sensor);
			return ess_att_error_from_errno(-ERANGE);
		}

		uint16_t co2 = fresh_data.eco2;
		co2_sensor.value = (int32_t)ess_encode_co2(co2);
		co2_sensor.value_known = true;
		LOG_DBG("Fresh CO2 update: %d ppm", co2);
	}

	/* Use the generic u16 read function for the actual data transfer */
	return read_u16(conn, attr, buf, len, offset);
}

static ssize_t read_tvoc(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
			 uint16_t len, uint16_t offset)
{
	if (!sensor_manager_is_ccs811_ready()) {
		/* CCS811 still conditioning - report 0xFFFF (value not known) per BT SIG */
		tvoc_sensor.value = ESS_UNKNOWN_U16;
		tvoc_sensor.value_known = true;
	} else {
		struct sensor_data fresh_data;

		sensor_manager_update_air_quality_for_ble();

		int ret = sensor_manager_get_data(&fresh_data);
		if (ret != 0 || !SENSOR_DATA_IS_VALID(&fresh_data, SENSOR_AIR_QUALITY)) {
			LOG_WRN("Failed to fetch valid TVOC data: %d", ret);
			invalidate_sensor(&tvoc_sensor);
			return ess_att_error_from_errno(ret != 0 ? ret : -ENODATA);
		}

		if (!ess_tvoc_in_range(fresh_data.tvoc)) {
			LOG_ERR("TVOC %u out of range", fresh_data.tvoc);
			invalidate_sensor(&tvoc_sensor);
			return ess_att_error_from_errno(-ERANGE);
		}

		uint16_t tvoc = fresh_data.tvoc;
		tvoc_sensor.value = (int32_t)ess_encode_tvoc(tvoc);
		tvoc_sensor.value_known = true;
		LOG_DBG("Fresh TVOC update: %d ppb", tvoc);
	}

	/* Use the generic u16 read function for the actual data transfer */
	return read_u16(conn, attr, buf, len, offset);
}

static ssize_t read_u32(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
			uint16_t len, uint16_t offset)
{
	const int32_t *value_ptr = attr->user_data;

	/* Trigger fresh sensor reading for pressure */
	if (value_ptr == &pressure_sensor.value) {
		struct sensor_data fresh_data;
		int ret = sensor_manager_update_selective(SENSOR_PRESSURE);

		if (ret != 0) {
			LOG_WRN("Failed to trigger pressure update: %d", ret);
			invalidate_sensor(&pressure_sensor);
			return ess_att_error_from_errno(ret);
		}

		ret = sensor_manager_get_data(&fresh_data);
		if (ret != 0 || !SENSOR_DATA_IS_VALID(&fresh_data, SENSOR_PRESSURE)) {
			LOG_WRN("Failed to fetch valid pressure data: %d", ret);
			invalidate_sensor(&pressure_sensor);
			return ess_att_error_from_errno(ret != 0 ? ret : -ENODATA);
		}

		if (!ess_pressure_in_range(fresh_data.pressure)) {
			LOG_ERR("Pressure %.3f kPa out of range", (double)fresh_data.pressure);
			invalidate_sensor(&pressure_sensor);
			return ess_att_error_from_errno(-ERANGE);
		}

		pressure_sensor.value = (int32_t)ess_encode_pressure(fresh_data.pressure);
		pressure_sensor.value_known = true;
		LOG_DBG("Fresh pressure update: P=%.3f kPa", (double)fresh_data.pressure);
	}

	if (!pressure_sensor.value_known) {
		return ess_att_error_from_errno(-ENODATA);
	}

	/* Pressure is uint32 in Pascals with 0.1 Pa resolution */
	uint32_t value = (uint32_t)(*value_ptr);
	uint32_t le_value = sys_cpu_to_le32(value);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &le_value, sizeof(le_value));
}

/* BLE ES Measurement Descriptor wire format sizes */
#define ESS_BLE_24BIT_FIELD_LEN 3U /* 24-bit fields use 3 bytes in little-endian BLE format */

struct read_es_measurement_rp {
	uint16_t flags; /* Reserved for Future Use */
	uint8_t sampling_function;
	uint8_t measurement_period[ESS_BLE_24BIT_FIELD_LEN];
	uint8_t update_interval[ESS_BLE_24BIT_FIELD_LEN];
	uint8_t application;
	uint8_t measurement_uncertainty;
} __packed;

static ssize_t read_es_measurement(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
				   uint16_t len, uint16_t offset)
{
	const struct es_measurement *value = attr->user_data;
	struct read_es_measurement_rp rsp;

	rsp.flags = sys_cpu_to_le16(value->flags);
	rsp.sampling_function = (uint8_t)value->sampling_func;
	sys_put_le24(value->meas_period, rsp.measurement_period);
	sys_put_le24(value->update_interval, rsp.update_interval);
	rsp.application = (uint8_t)value->application;
	rsp.measurement_uncertainty = value->meas_uncertainty;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, &rsp, sizeof(rsp));
}

static ssize_t read_valid_range(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset)
{
	const struct ess_sensor *sensor = attr->user_data;

	/* Check if this is the pressure sensor (which uses uint32 ranges) */
	if (sensor == &pressure_sensor) {
		uint32_t tmp[] = {sys_cpu_to_le32((uint32_t)sensor->lower_limit),
				  sys_cpu_to_le32((uint32_t)sensor->upper_limit)};
		return bt_gatt_attr_read(conn, attr, buf, len, offset, tmp, sizeof(tmp));
	} else {
		/* For other sensors (temperature, humidity, CO2, TVOC) use uint16 */
		uint16_t tmp[] = {sys_cpu_to_le16((int16_t)sensor->lower_limit),
				  sys_cpu_to_le16((int16_t)sensor->upper_limit)};
		return bt_gatt_attr_read(conn, attr, buf, len, offset, tmp, sizeof(tmp));
	}
}

/* CCC change handlers */
static void temp_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	(void)attr;
	temperature_sensor.notify_enabled = (value == BT_GATT_CCC_NOTIFY);
	LOG_INF("Temperature notifications %s",
		temperature_sensor.notify_enabled ? "enabled" : "disabled");
}

static void humidity_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	(void)attr;
	humidity_sensor.notify_enabled = (value == BT_GATT_CCC_NOTIFY);
	LOG_INF("Humidity notifications %s",
		humidity_sensor.notify_enabled ? "enabled" : "disabled");
}

static void pressure_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	(void)attr;
	pressure_sensor.notify_enabled = (value == BT_GATT_CCC_NOTIFY);
	LOG_INF("Pressure notifications %s",
		pressure_sensor.notify_enabled ? "enabled" : "disabled");
}

static void co2_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	(void)attr;
	co2_sensor.notify_enabled = (value == BT_GATT_CCC_NOTIFY);
	LOG_INF("CO2 notifications %s", co2_sensor.notify_enabled ? "enabled" : "disabled");
}

static void tvoc_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	(void)attr;
	tvoc_sensor.notify_enabled = (value == BT_GATT_CCC_NOTIFY);
	LOG_INF("TVOC notifications %s", tvoc_sensor.notify_enabled ? "enabled" : "disabled");
}

/* ESS GATT Service Definition */
BT_GATT_SERVICE_DEFINE(
	ess_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_ESS),

	/* Temperature Characteristic */
	BT_GATT_CHARACTERISTIC(BT_UUID_TEMPERATURE, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ, read_u16, NULL, &temperature_sensor.value),
	BT_GATT_DESCRIPTOR(BT_UUID_ES_MEASUREMENT, BT_GATT_PERM_READ, read_es_measurement, NULL,
			   &temperature_sensor.meas),
	BT_GATT_CUD("Temperature", BT_GATT_PERM_READ),
	BT_GATT_DESCRIPTOR(BT_UUID_VALID_RANGE, BT_GATT_PERM_READ, read_valid_range, NULL,
			   &temperature_sensor),
	BT_GATT_CCC(temp_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	/* Humidity Characteristic */
	BT_GATT_CHARACTERISTIC(BT_UUID_HUMIDITY, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ, read_u16, NULL, &humidity_sensor.value),
	BT_GATT_DESCRIPTOR(BT_UUID_ES_MEASUREMENT, BT_GATT_PERM_READ, read_es_measurement, NULL,
			   &humidity_sensor.meas),
	BT_GATT_CUD("Humidity", BT_GATT_PERM_READ),
	BT_GATT_DESCRIPTOR(BT_UUID_VALID_RANGE, BT_GATT_PERM_READ, read_valid_range, NULL,
			   &humidity_sensor),
	BT_GATT_CCC(humidity_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	/* Pressure Characteristic */
	BT_GATT_CHARACTERISTIC(BT_UUID_PRESSURE, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ, read_u32, NULL, &pressure_sensor.value),
	BT_GATT_DESCRIPTOR(BT_UUID_ES_MEASUREMENT, BT_GATT_PERM_READ, read_es_measurement, NULL,
			   &pressure_sensor.meas),
	BT_GATT_CUD("Pressure", BT_GATT_PERM_READ),
	BT_GATT_DESCRIPTOR(BT_UUID_VALID_RANGE, BT_GATT_PERM_READ, read_valid_range, NULL,
			   &pressure_sensor),
	BT_GATT_CCC(pressure_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	/* CO2 Concentration Characteristic */
	BT_GATT_CHARACTERISTIC(BT_UUID_GATT_CO2CONC, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ, read_co2, NULL, &co2_sensor.value),
	BT_GATT_DESCRIPTOR(BT_UUID_ES_MEASUREMENT, BT_GATT_PERM_READ, read_es_measurement, NULL,
			   &co2_sensor.meas),
	BT_GATT_CUD("CO2 Concentration", BT_GATT_PERM_READ),
	BT_GATT_DESCRIPTOR(BT_UUID_VALID_RANGE, BT_GATT_PERM_READ, read_valid_range, NULL,
			   &co2_sensor),
	BT_GATT_CCC(co2_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	/* TVOC Concentration Characteristic */
	BT_GATT_CHARACTERISTIC(BT_UUID_GATT_VOCCONC, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ, read_tvoc, NULL, &tvoc_sensor.value),
	BT_GATT_DESCRIPTOR(BT_UUID_ES_MEASUREMENT, BT_GATT_PERM_READ, read_es_measurement, NULL,
			   &tvoc_sensor.meas),
	BT_GATT_CUD("TVOC Concentration", BT_GATT_PERM_READ),
	BT_GATT_DESCRIPTOR(BT_UUID_VALID_RANGE, BT_GATT_PERM_READ, read_valid_range, NULL,
			   &tvoc_sensor),
	BT_GATT_CCC(tvoc_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), );

/* Type-safe update functions for each sensor type */
static void update_sensor_u16(struct ess_sensor *sensor, uint16_t new_value,
			      const struct bt_gatt_attr *chrc_attr)
{
	bool notify = ess_should_notify_16(sensor->condition, (int16_t)sensor->value,
					   (int16_t)new_value, (int16_t)sensor->ref_val);

	sensor->value = (int32_t)new_value;
	sensor->value_known = true;

	if (notify && sensor->notify_enabled) {
		uint16_t value = sys_cpu_to_le16(new_value);
		int ret = bt_gatt_notify(NULL, chrc_attr, &value, sizeof(value));
		if (ret) {
			LOG_WRN("Failed to send u16 notification: %d", ret);
		} else {
			LOG_DBG("u16 notification sent: %u", new_value);
		}
	}
}

static void update_sensor_i16(struct ess_sensor *sensor, int16_t new_value,
			      const struct bt_gatt_attr *chrc_attr)
{
	bool notify = ess_should_notify_16(sensor->condition, (int16_t)sensor->value, new_value,
					   (int16_t)sensor->ref_val);

	sensor->value = (int32_t)new_value;
	sensor->value_known = true;

	if (notify && sensor->notify_enabled) {
		uint16_t value = sys_cpu_to_le16((uint16_t)new_value);
		int ret = bt_gatt_notify(NULL, chrc_attr, &value, sizeof(value));
		if (ret) {
			LOG_WRN("Failed to send i16 notification: %d", ret);
		} else {
			LOG_DBG("i16 notification sent: %d", new_value);
		}
	}
}

static void update_sensor_u32(struct ess_sensor *sensor, uint32_t new_value,
			      const struct bt_gatt_attr *chrc_attr)
{
	bool notify = ess_should_notify_32(sensor->condition, sensor->value, (int32_t)new_value,
					   sensor->ref_val);

	sensor->value = (int32_t)new_value;
	sensor->value_known = true;

	if (notify && sensor->notify_enabled) {
		uint32_t value = sys_cpu_to_le32(new_value);
		int ret = bt_gatt_notify(NULL, chrc_attr, &value, sizeof(value));
		if (ret) {
			LOG_WRN("Failed to send u32 notification: %d", ret);
		} else {
			LOG_DBG("u32 notification sent: %u", new_value);
		}
	}
}

/* Sensor data callback to automatically update ESS */
static void sensor_data_callback(const struct sensor_data *data)
{
	if (ess_initialized && data) {
		ess_service_update(data);
	}
}

int ess_service_init(void)
{
	LOG_INF("Initializing Environmental Sensing Service");

	/* Service is automatically registered via BT_GATT_SERVICE_DEFINE */
	ess_initialized = true;

	/* Register callback to automatically update ESS when sensor data changes */
	int ret = sensor_manager_register_callback(sensor_data_callback);
	if (ret) {
		LOG_WRN("Failed to register sensor callback: %d", ret);
	} else {
		LOG_DBG("ESS registered for automatic sensor data updates");
	}

	LOG_INF("ESS initialized with 5 characteristics: Temperature, Humidity, "
		"Pressure, CO2, TVOC");
	return 0;
}

int ess_service_update(const struct sensor_data *data)
{
	if (!ess_initialized) {
		LOG_ERR("ESS service not initialized");
		return -ENODEV;
	}

	if (!data) {
		LOG_ERR("Invalid sensor data");
		return -EINVAL;
	}

	LOG_DBG("Updating ESS with sensor data");

	/* Update temperature (convert from °C to 0.01°C as per Bluetooth SIG spec) */
	if ((data->valid_mask & SENSOR_TEMPERATURE) != 0) {
		if (!ess_temperature_in_range(data->temperature)) {
			invalidate_sensor(&temperature_sensor);
			LOG_ERR("Temperature %.2f out of range", (double)data->temperature);
			return -ERANGE;
		}
		int16_t temp_value = ess_encode_temperature(data->temperature);
		update_sensor_i16(&temperature_sensor, temp_value,
				  &ess_svc.attrs[ESS_ATTR_IDX_TEMPERATURE]);
	} else {
		invalidate_sensor(&temperature_sensor);
	}

	/* Update humidity using centralized conversion */
	if ((data->valid_mask & SENSOR_HUMIDITY) != 0) {
		if (!ess_humidity_in_range(data->humidity)) {
			invalidate_sensor(&humidity_sensor);
			LOG_ERR("Humidity %.2f out of range", (double)data->humidity);
			return -ERANGE;
		}
		uint16_t humid_value = ess_encode_humidity(data->humidity);
		update_sensor_u16(&humidity_sensor, humid_value,
				  &ess_svc.attrs[ESS_ATTR_IDX_HUMIDITY]);
	} else {
		invalidate_sensor(&humidity_sensor);
	}

	/* Update pressure using centralized conversion */
	if ((data->valid_mask & SENSOR_PRESSURE) != 0) {
		if (!ess_pressure_in_range(data->pressure)) {
			invalidate_sensor(&pressure_sensor);
			LOG_ERR("Pressure %.3f kPa out of range", (double)data->pressure);
			return -ERANGE;
		}
		uint32_t pressure_pa = ess_encode_pressure(data->pressure);
		update_sensor_u32(&pressure_sensor, pressure_pa,
				  &ess_svc.attrs[ESS_ATTR_IDX_PRESSURE]);
	} else {
		invalidate_sensor(&pressure_sensor);
	}

	/* Update CO2 concentration using centralized conversion */
	if ((data->valid_mask & SENSOR_AIR_QUALITY) != 0 && sensor_manager_is_ccs811_ready()) {
		if (!ess_co2_in_range(data->eco2) || !ess_tvoc_in_range(data->tvoc)) {
			invalidate_sensor(&co2_sensor);
			invalidate_sensor(&tvoc_sensor);
			LOG_ERR("Air quality out of range: CO2=%u TVOC=%u", data->eco2, data->tvoc);
			return -ERANGE;
		}
		/* CCS811 is ready and data is valid - use real reading */
		uint16_t co2_value = ess_encode_co2(data->eco2);
		update_sensor_u16(&co2_sensor, co2_value, &ess_svc.attrs[ESS_ATTR_IDX_CO2]);
	} else if (!sensor_manager_is_ccs811_ready()) {
		/* CCS811 still conditioning - report 0xFFFF (value not known) per BT SIG */
		update_sensor_u16(&co2_sensor, ESS_UNKNOWN_U16, &ess_svc.attrs[ESS_ATTR_IDX_CO2]);
	} else {
		invalidate_sensor(&co2_sensor);
	}
	/* If sensor is ready but data invalid, keep last valid reading */

	/* Update TVOC concentration using centralized conversion */
	if ((data->valid_mask & SENSOR_AIR_QUALITY) != 0 && sensor_manager_is_ccs811_ready()) {
		/* CCS811 is ready and data is valid - use real reading */
		uint16_t tvoc_value = ess_encode_tvoc(data->tvoc);
		update_sensor_u16(&tvoc_sensor, tvoc_value, &ess_svc.attrs[ESS_ATTR_IDX_TVOC]);
	} else if (!sensor_manager_is_ccs811_ready()) {
		/* CCS811 still conditioning - report 0xFFFF (value not known) per BT SIG */
		update_sensor_u16(&tvoc_sensor, ESS_UNKNOWN_U16, &ess_svc.attrs[ESS_ATTR_IDX_TVOC]);
	} else {
		invalidate_sensor(&tvoc_sensor);
	}
	/* If sensor is ready but data invalid, keep last valid reading */

	/* Log current values - show conditioning status for gas sensors */
	if ((data->valid_mask & SENSOR_AIR_QUALITY) != 0) {
		LOG_INF("ESS updated: T=%.2f°C, H=%.1f%%, P=%.3fkPa(%.2fhPa), CO2=%dppm, TVOC=%dppb",
			(double)data->temperature, (double)data->humidity, (double)data->pressure,
			(double)(data->pressure * 10.0f), data->eco2, data->tvoc);
	} else if (sensor_manager_is_ccs811_ready()) {
		LOG_INF("ESS updated: T=%.2f°C, H=%.1f%%, P=%.3fkPa(%.2fhPa), CO2/TVOC=sensor_error",
			(double)data->temperature, (double)data->humidity, (double)data->pressure,
			(double)(data->pressure * 10.0f));
	} else {
		int64_t elapsed = k_uptime_get();
		int64_t remaining_sec = (CCS811_CONDITIONING_MS - elapsed) / ESS_MS_PER_SEC;
		if (remaining_sec > 0) {
			LOG_INF("ESS updated: T=%.2f°C, H=%.1f%%, P=%.3fkPa(%.2fhPa), "
				"CO2/TVOC=conditioning(%ds)",
				(double)data->temperature, (double)data->humidity,
				(double)data->pressure, (double)(data->pressure * 10.0f),
				(int)remaining_sec);
		} else {
			LOG_INF("ESS updated: T=%.2f°C, H=%.1f%%, P=%.3fkPa(%.2fhPa), "
				"CO2/TVOC=conditioning(finishing)",
				(double)data->temperature, (double)data->humidity,
				(double)data->pressure, (double)(data->pressure * 10.0f));
		}
	}

	return 0;
}

int ess_service_get_temperature(void)
{
	return temperature_sensor.value_known ? temperature_sensor.value : -ENODATA;
}

int ess_service_get_humidity(void)
{
	return humidity_sensor.value_known ? humidity_sensor.value : -ENODATA;
}

int ess_service_get_pressure(void)
{
	return pressure_sensor.value_known ? pressure_sensor.value : -ENODATA;
}

int ess_service_get_co2(void)
{
	return co2_sensor.value_known ? co2_sensor.value : -ENODATA;
}

int ess_service_get_tvoc(void)
{
	return tvoc_sensor.value_known ? tvoc_sensor.value : -ENODATA;
}
