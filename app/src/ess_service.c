/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include "ess_service.h"
#include "sensor_manager.h"

LOG_MODULE_REGISTER(ess_service, CONFIG_LOG_DEFAULT_LEVEL);

/* ESS Trigger Setting conditions - Bluetooth SIG standard definitions */
#define ESS_TRIGGER_INACTIVE 0x00
#define ESS_TRIGGER_FIXED_TIME_INTERVAL 0x01
#define ESS_TRIGGER_NO_LESS_THAN_SPECIFIED_TIME 0x02
#define ESS_TRIGGER_FIXED_TIME_INTERVAL 0x01
#define ESS_TRIGGER_NO_LESS_THAN_SPECIFIED_TIME 0x02
#define ESS_TRIGGER_VALUE_CHANGED 0x03
#define ESS_TRIGGER_LESS_THAN_REF_VALUE 0x04
#define ESS_TRIGGER_LESS_OR_EQUAL_TO_REF_VALUE 0x05
#define ESS_TRIGGER_GREATER_THAN_REF_VALUE 0x06
#define ESS_TRIGGER_GREATER_OR_EQUAL_TO_REF_VALUE 0x07
#define ESS_TRIGGER_EQUAL_TO_REF_VALUE 0x08
#define ESS_TRIGGER_NOT_EQUAL_TO_REF_VALUE 0x09

/* ESS Measurement Descriptor – Sampling Functions - Bluetooth SIG standard */
#define ESS_DESC_SAMPLING_UNSPECIFIED 0x00
#define ESS_DESC_SAMPLING_INSTANTANEOUS 0x01
#define ESS_DESC_SAMPLING_ARITHMETIC_MEAN 0x02
#define ESS_DESC_SAMPLING_RMS 0x03
#define ESS_DESC_SAMPLING_MAXIMUM 0x04
#define ESS_DESC_SAMPLING_MINIMUM 0x05
#define ESS_DESC_SAMPLING_ACCUMULATED 0x06
#define ESS_DESC_SAMPLING_COUNT 0x07
#define ESS_DESC_SAMPLING_RMS 0x03
#define ESS_DESC_SAMPLING_MAXIMUM 0x04
#define ESS_DESC_SAMPLING_MINIMUM 0x05
#define ESS_DESC_SAMPLING_ACCUMULATED 0x06
#define ESS_DESC_SAMPLING_COUNT 0x07

/* ES Measurement Descriptor - Applications - Bluetooth SIG standard */
#define ESS_DESC_APP_UNSPECIFIED 0x00
#define ESS_DESC_APP_AIR 0x01
#define ESS_DESC_APP_OUTDOOR 0x13
#define ESS_DESC_APP_OUTDOOR 0x13
#define ESS_DESC_APP_INDOOR 0x14

#define BT_UUID_CO2_CONCENTRATION_VAL 0x2B8C
#define BT_UUID_TVOC_CONCENTRATION_VAL 0x2BE7

#define BT_UUID_CO2_CONCENTRATION                                              \
  BT_UUID_DECLARE_16(BT_UUID_CO2_CONCENTRATION_VAL)
#define BT_UUID_TVOC_CONCENTRATION                                             \
  BT_UUID_DECLARE_16(BT_UUID_TVOC_CONCENTRATION_VAL)

/* Sensor valid ranges according to Bluetooth SIG specifications */
#define TEMP_LOWER_LIMIT -27315    /* -273.15°C in 0.01°C units (sint16) */
#define TEMP_UPPER_LIMIT 32767     /* Max sint16 */
#define HUMIDITY_LOWER_LIMIT 0U    /* 0% in 0.01% units (uint16) */
#define HUMIDITY_UPPER_LIMIT 10000U /* 100.00% in 0.01% units (uint16) */
#define PRESSURE_LOWER_LIMIT 30000UL /* 300.0 hPa = 30000 Pa in 0.1 Pa units (uint32) */
#define PRESSURE_UPPER_LIMIT 120000UL /* 1200.0 hPa = 120000 Pa in 0.1 Pa units (uint32) */
#define CO2_LOWER_LIMIT 400U       /* 400 ppm (uint16) */
#define CO2_UPPER_LIMIT 5000U      /* 5000 ppm (uint16) */
#define TVOC_LOWER_LIMIT 0U        /* 0 ppb (uint16) */
#define TVOC_UPPER_LIMIT 32767U    /* Max for uint16 practical range */

/* Environmental Sensing Service measurements - Bluetooth SIG ESS Standard */
struct es_measurement {
  uint16_t flags;           /* Reserved for Future Use - always 0x0000 per SIG spec */
  uint8_t sampling_func;    /* Sampling Function field - how the measurement is obtained:
                             * 0x00 = Unspecified
                             * 0x01 = Instantaneous (single point-in-time measurement)
                             * 0x02 = Arithmetic Mean (average over measurement period)
                             * 0x03 = RMS (root mean square over measurement period)
                             * 0x04 = Maximum (peak value over measurement period)
                             * 0x05 = Minimum (lowest value over measurement period)
                             * 0x06 = Accumulated (cumulative sum over measurement period)
                             * 0x07 = Count (number of measurements in measurement period) */
  uint32_t meas_period;     /* Measurement Period - time in seconds over which sampling occurs:
                             * 0x000000 = Not applicable (for instantaneous measurements)
                             * 0x000001-0xFFFFFF = Period in seconds (1 sec to ~194 days) */
  uint32_t update_interval; /* Update Interval - time in seconds between measurements:
                             * 0x000000 = Not applicable (for on-demand/event-driven updates)
                             * 0x000001-0xFFFFFF = Interval in seconds (1 sec to ~194 days) */
  uint8_t application;      /* Application field - describes measurement context:
                             * 0x00 = Unspecified
                             * 0x01 = Air (atmospheric/ambient air measurements)
                             * 0x13 = Outdoor (external environmental conditions)
                             * 0x14 = Indoor (internal environmental conditions) */
  uint8_t meas_uncertainty; /* Measurement Uncertainty - accuracy/precision information:
                             * Expressed in same units as the measurement
                             * Represents ± uncertainty range (e.g., ±0.5°C = 0x32 for temp) */
};

/* Sensor data structures */
struct ess_sensor {
  int32_t value;        /* Current sensor value - stored in characteristic's native units:
                         * Temperature: 0.01°C resolution (2000 = 20.00°C)
                         * Humidity: 0.01% resolution (5000 = 50.00%)
                         * Pressure: 0.1 Pa resolution (101325 = 1013.25 hPa)
                         * CO2: 1 ppm resolution (400 = 400 ppm)
                         * TVOC: 1 ppb resolution (0 = 0 ppb) */
  int32_t lower_limit;  /* Valid Range Lower Limit - minimum acceptable value */
  int32_t upper_limit;  /* Valid Range Upper Limit - maximum acceptable value */
  uint8_t condition;    /* Trigger Setting Condition - when to send notifications:
                         * ESS_TRIGGER_INACTIVE = No notifications
                         * ESS_TRIGGER_VALUE_CHANGED = Notify on any value change
                         * ESS_TRIGGER_LESS_THAN_REF_VALUE = Notify when < ref_val
                         * ESS_TRIGGER_GREATER_THAN_REF_VALUE = Notify when > ref_val
                         * etc. (see ESS_TRIGGER_* macros above) */
  int32_t ref_val;      /* Trigger Setting Reference Value - comparison value for conditional triggers */
  struct es_measurement meas; /* ES Measurement Descriptor - describes how measurement is obtained */
  bool notify_enabled;  /* Client Characteristic Configuration - true if client enabled notifications */
};

/* ESS sensors state */
static struct ess_sensor temperature_sensor = {
    .value = 2000, /* 20.00°C in 0.01°C units */
    .lower_limit = TEMP_LOWER_LIMIT,
    .upper_limit = TEMP_UPPER_LIMIT,
    .condition = ESS_TRIGGER_VALUE_CHANGED,
    .ref_val = 0,
    .meas =
        {
            .flags = 0,
            .sampling_func = ESS_DESC_SAMPLING_INSTANTANEOUS,
            .meas_period = 0x00,     /* On-demand readings */
            .update_interval = 0x00, /* No fixed interval - read on demand */
            .application = ESS_DESC_APP_INDOOR,
            .meas_uncertainty = 0x32, /* ±0.50°C in 0.01°C units (50) */
        },
    .notify_enabled = false,
};

static struct ess_sensor humidity_sensor = {
    .value = 5000, /* 50.00% in 0.01% units */
    .lower_limit = HUMIDITY_LOWER_LIMIT,
    .upper_limit = HUMIDITY_UPPER_LIMIT,
    .condition = ESS_TRIGGER_VALUE_CHANGED,
    .ref_val = 0,
    .meas =
        {
            .flags = 0,
            .sampling_func = ESS_DESC_SAMPLING_INSTANTANEOUS,
            .meas_period = 0x00,     /* On-demand readings */
            .update_interval = 0x00, /* No fixed interval - read on demand */
            .application = ESS_DESC_APP_INDOOR,
            .meas_uncertainty = 0x32, /* ±0.50%RH in 0.01% units (50) */
        },
    .notify_enabled = false,
};

static struct ess_sensor pressure_sensor = {
    .value = 101325, /* 1013.25 hPa = 101325 Pa in 0.1 Pa units */
    .lower_limit = PRESSURE_LOWER_LIMIT,
    .upper_limit = PRESSURE_UPPER_LIMIT,
    .condition = ESS_TRIGGER_VALUE_CHANGED,
    .ref_val = 0,
    .meas =
        {
            .flags = 0,
            .sampling_func = ESS_DESC_SAMPLING_INSTANTANEOUS,
            .meas_period = 0x00,     /* On-demand readings */
            .update_interval = 0x00, /* No fixed interval - read on demand */
            .application = ESS_DESC_APP_INDOOR,
            .meas_uncertainty = 0x96, /* ±15.0 Pa in 0.1 Pa units (150 * 0.1 Pa = 15.0 Pa = 0.15 hPa) */
        },
    .notify_enabled = false,
};

static struct ess_sensor co2_sensor = {
    .value = 0, /* 0 ppm - indicates sensor not ready/conditioning */
    .lower_limit = CO2_LOWER_LIMIT,
    .upper_limit = CO2_UPPER_LIMIT,
    .condition = ESS_TRIGGER_VALUE_CHANGED,
    .ref_val = 0,
    .meas =
        {
            .flags = 0,
            .sampling_func = ESS_DESC_SAMPLING_INSTANTANEOUS,
            .meas_period = 0x00,     /* On-demand readings - no averaging period */
            .update_interval = 0x00, /* No fixed interval - read on demand */
            .application = ESS_DESC_APP_AIR,
            .meas_uncertainty = 0x64, /* ±100 ppm (typical for CCS811) */
        },
    .notify_enabled = false,
};

static struct ess_sensor tvoc_sensor = {
    .value = 0, /* 0 ppb - indicates sensor not ready/conditioning */
    .lower_limit = TVOC_LOWER_LIMIT,
    .upper_limit = TVOC_UPPER_LIMIT,
    .condition = ESS_TRIGGER_VALUE_CHANGED,
    .ref_val = 0,
    .meas =
        {
            .flags = 0,
            .sampling_func = ESS_DESC_SAMPLING_INSTANTANEOUS,
            .meas_period = 0x00,     /* On-demand readings - no averaging period */
            .update_interval = 0x00, /* No fixed interval - read on demand */
            .application = ESS_DESC_APP_AIR,
            .meas_uncertainty = 0x32, /* ±50 ppb (reasonable for TVOC sensor) */
        },
    .notify_enabled = false,
};

static bool ess_initialized = false;

/* GATT attribute indices for characteristics */
#define TEMP_CHAR_ATTR_IDX    1   /* Temperature characteristic */
#define HUMIDITY_CHAR_ATTR_IDX 7   /* Humidity characteristic */
#define PRESSURE_CHAR_ATTR_IDX 13  /* Pressure characteristic */
#define CO2_CHAR_ATTR_IDX     19   /* CO2 characteristic */
#define TVOC_CHAR_ATTR_IDX    25   /* TVOC characteristic */

/* Conversion helper functions - centralized data conversion logic */
static int16_t convert_temperature_to_ble(float temp_celsius) {
  int16_t result = (int16_t)(temp_celsius * 100);  /* Convert to 0.01°C units */
  LOG_DBG("Temperature conversion: %.2f°C -> %d (0.01°C units)", (double)temp_celsius, result);
  return result;
}

static uint16_t convert_humidity_to_ble(float humidity_percent) {
  uint16_t result = (uint16_t)(humidity_percent * 100);  /* Convert to 0.01% units */
  LOG_DBG("Humidity conversion: %.2f%% -> %u (0.01%% units)", (double)humidity_percent, result);
  return result;
}

static uint32_t convert_pressure_to_ble(float pressure_hpa) {
  /* Convert hPa to Pa with 0.1 Pa resolution: hPa * 100 / 0.1 = hPa * 1000 */
  uint32_t pressure_pa = (uint32_t)(pressure_hpa * 1000);

  /* Clamp to valid range */
  if (pressure_pa < PRESSURE_LOWER_LIMIT) pressure_pa = PRESSURE_LOWER_LIMIT;
  if (pressure_pa > PRESSURE_UPPER_LIMIT) pressure_pa = PRESSURE_UPPER_LIMIT;

  LOG_DBG("Pressure conversion: %.1fhPa -> %u Pa (0.1Pa units)", (double)pressure_hpa, pressure_pa);
  return pressure_pa;
}

static uint16_t convert_co2_to_ble(uint16_t co2_ppm) {
  return co2_ppm;  /* Direct conversion - already in ppm */
}

static uint16_t convert_tvoc_to_ble(uint16_t tvoc_ppb) {
  return tvoc_ppb;  /* Direct conversion - already in ppb */
}

/* Helper functions */
static ssize_t read_u16(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                        void *buf, uint16_t len, uint16_t offset) {
  const int32_t *value_ptr = attr->user_data;

  /* Trigger fresh sensor reading based on which characteristic is being read */
  if (value_ptr == &temperature_sensor.value || value_ptr == &humidity_sensor.value) {
    /* Temperature or Humidity - trigger fresh temp/humidity sensor update */
    int ret = sensor_manager_update_selective(SENSOR_TEMPERATURE | SENSOR_HUMIDITY);
    if (ret == 0) {
      /* Get fresh data and update local values using centralized conversion */
      float temp = sensor_manager_get_temperature();
      float humid = sensor_manager_get_humidity();
      temperature_sensor.value = (int32_t)convert_temperature_to_ble(temp);
      humidity_sensor.value = (int32_t)convert_humidity_to_ble(humid);
      LOG_DBG("Fresh temp/humidity update: T=%.2f°C, H=%.1f%%", (double)temp, (double)humid);
    } else {
      LOG_WRN("Failed to trigger temp/humidity update: %d", ret);
    }
  } else if (value_ptr == &pressure_sensor.value) {
    /* Pressure - trigger fresh pressure sensor update */
    int ret = sensor_manager_update_selective(SENSOR_PRESSURE);
    if (ret == 0) {
      float pressure = sensor_manager_get_pressure();
      pressure_sensor.value = (int32_t)convert_pressure_to_ble(pressure);
      LOG_DBG("Fresh pressure update: P=%.1fhPa", (double)pressure);
    } else {
      LOG_WRN("Failed to trigger pressure update: %d", ret);
    }
  }

  /* Ensure value is within int16_t range before conversion */
  int16_t value = (int16_t)(*value_ptr & 0xFFFF);
  uint16_t le_value = sys_cpu_to_le16((uint16_t)value);

  return bt_gatt_attr_read(conn, attr, buf, len, offset, &le_value,
                           sizeof(le_value));
}

static ssize_t read_co2(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                        void *buf, uint16_t len, uint16_t offset) {
  /* Trigger fresh air quality update - just CO2/TVOC sensors */
  sensor_manager_update_selective(SENSOR_AIR_QUALITY);

  /* Update local CO2 value with fresh data using centralized conversion */
  uint16_t co2 = sensor_manager_get_eco2();
  co2_sensor.value = (int32_t)convert_co2_to_ble(co2);
  LOG_DBG("Fresh CO2 update: %d ppm", co2);

  /* Use the generic u16 read function for the actual data transfer */
  return read_u16(conn, attr, buf, len, offset);
}

static ssize_t read_tvoc(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                         void *buf, uint16_t len, uint16_t offset) {
  /* Trigger fresh air quality update - just CO2/TVOC sensors */
  sensor_manager_update_selective(SENSOR_AIR_QUALITY);

  /* Update local TVOC value with fresh data using centralized conversion */
  uint16_t tvoc = sensor_manager_get_tvoc();
  tvoc_sensor.value = (int32_t)convert_tvoc_to_ble(tvoc);
  LOG_DBG("Fresh TVOC update: %d ppb", tvoc);

  /* Use the generic u16 read function for the actual data transfer */
  return read_u16(conn, attr, buf, len, offset);
}

static ssize_t read_u32(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                        void *buf, uint16_t len, uint16_t offset) {
  const int32_t *value_ptr = attr->user_data;

  /* Trigger fresh sensor reading for pressure */
  if (value_ptr == &pressure_sensor.value) {
    int ret = sensor_manager_update_selective(SENSOR_PRESSURE);
    if (ret == 0) {
      float pressure = sensor_manager_get_pressure();
      pressure_sensor.value = (int32_t)convert_pressure_to_ble(pressure);
      LOG_DBG("Fresh pressure update: P=%.1fhPa", (double)pressure);
    } else {
      LOG_WRN("Failed to trigger pressure update: %d", ret);
    }
  }

  /* Pressure is uint32 in Pascals with 0.1 Pa resolution */
  uint32_t value = (uint32_t)(*value_ptr);
  uint32_t le_value = sys_cpu_to_le32(value);

  return bt_gatt_attr_read(conn, attr, buf, len, offset, &le_value,
                           sizeof(le_value));
}

struct read_es_measurement_rp {
  uint16_t flags; /* Reserved for Future Use */
  uint8_t sampling_function;
  uint8_t measurement_period[3];
  uint8_t update_interval[3];
  uint8_t application;
  uint8_t measurement_uncertainty;
} __packed;

static ssize_t read_es_measurement(struct bt_conn *conn,
                                   const struct bt_gatt_attr *attr, void *buf,
                                   uint16_t len, uint16_t offset) {
  const struct es_measurement *value = attr->user_data;
  struct read_es_measurement_rp rsp;

  rsp.flags = sys_cpu_to_le16(value->flags);
  rsp.sampling_function = value->sampling_func;
  sys_put_le24(value->meas_period, rsp.measurement_period);
  sys_put_le24(value->update_interval, rsp.update_interval);
  rsp.application = value->application;
  rsp.measurement_uncertainty = value->meas_uncertainty;

  return bt_gatt_attr_read(conn, attr, buf, len, offset, &rsp, sizeof(rsp));
}

static ssize_t read_valid_range(struct bt_conn *conn,
                                const struct bt_gatt_attr *attr, void *buf,
                                uint16_t len, uint16_t offset) {
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
static void temp_ccc_cfg_changed(const struct bt_gatt_attr *attr,
                                 uint16_t value) {
  temperature_sensor.notify_enabled = (value == BT_GATT_CCC_NOTIFY);
  LOG_INF("Temperature notifications %s",
          temperature_sensor.notify_enabled ? "enabled" : "disabled");
}

static void humidity_ccc_cfg_changed(const struct bt_gatt_attr *attr,
                                     uint16_t value) {
  humidity_sensor.notify_enabled = (value == BT_GATT_CCC_NOTIFY);
  LOG_INF("Humidity notifications %s",
          humidity_sensor.notify_enabled ? "enabled" : "disabled");
}

static void pressure_ccc_cfg_changed(const struct bt_gatt_attr *attr,
                                     uint16_t value) {
  pressure_sensor.notify_enabled = (value == BT_GATT_CCC_NOTIFY);
  LOG_INF("Pressure notifications %s",
          pressure_sensor.notify_enabled ? "enabled" : "disabled");
}

static void co2_ccc_cfg_changed(const struct bt_gatt_attr *attr,
                                uint16_t value) {
  co2_sensor.notify_enabled = (value == BT_GATT_CCC_NOTIFY);
  LOG_INF("CO2 notifications %s",
          co2_sensor.notify_enabled ? "enabled" : "disabled");
}

static void tvoc_ccc_cfg_changed(const struct bt_gatt_attr *attr,
                                 uint16_t value) {
  tvoc_sensor.notify_enabled = (value == BT_GATT_CCC_NOTIFY);
  LOG_INF("TVOC notifications %s",
          tvoc_sensor.notify_enabled ? "enabled" : "disabled");
}

/* ESS GATT Service Definition */
BT_GATT_SERVICE_DEFINE(
    ess_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_ESS),

    /* Temperature Characteristic */
    BT_GATT_CHARACTERISTIC(BT_UUID_TEMPERATURE,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ, read_u16, NULL,
                           &temperature_sensor.value),
    BT_GATT_DESCRIPTOR(BT_UUID_ES_MEASUREMENT, BT_GATT_PERM_READ,
                       read_es_measurement, NULL, &temperature_sensor.meas),
    BT_GATT_CUD("Temperature", BT_GATT_PERM_READ),
    BT_GATT_DESCRIPTOR(BT_UUID_VALID_RANGE, BT_GATT_PERM_READ, read_valid_range,
                       NULL, &temperature_sensor),
    BT_GATT_CCC(temp_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    /* Humidity Characteristic */
    BT_GATT_CHARACTERISTIC(BT_UUID_HUMIDITY,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ, read_u16, NULL,
                           &humidity_sensor.value),
    BT_GATT_DESCRIPTOR(BT_UUID_ES_MEASUREMENT, BT_GATT_PERM_READ,
                       read_es_measurement, NULL, &humidity_sensor.meas),
    BT_GATT_CUD("Humidity", BT_GATT_PERM_READ),
    BT_GATT_DESCRIPTOR(BT_UUID_VALID_RANGE, BT_GATT_PERM_READ, read_valid_range,
                       NULL, &humidity_sensor),
    BT_GATT_CCC(humidity_ccc_cfg_changed,
                BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    /* Pressure Characteristic */
    BT_GATT_CHARACTERISTIC(BT_UUID_PRESSURE,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ, read_u32, NULL,
                           &pressure_sensor.value),
    BT_GATT_DESCRIPTOR(BT_UUID_ES_MEASUREMENT, BT_GATT_PERM_READ,
                       read_es_measurement, NULL, &pressure_sensor.meas),
    BT_GATT_CUD("Pressure", BT_GATT_PERM_READ),
    BT_GATT_DESCRIPTOR(BT_UUID_VALID_RANGE, BT_GATT_PERM_READ, read_valid_range,
                       NULL, &pressure_sensor),
    BT_GATT_CCC(pressure_ccc_cfg_changed,
                BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    /* CO2 Concentration Characteristic */
    BT_GATT_CHARACTERISTIC(BT_UUID_CO2_CONCENTRATION,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ, read_co2, NULL,
                           &co2_sensor.value),
    BT_GATT_DESCRIPTOR(BT_UUID_ES_MEASUREMENT, BT_GATT_PERM_READ,
                       read_es_measurement, NULL, &co2_sensor.meas),
    BT_GATT_CUD("CO2 Concentration", BT_GATT_PERM_READ),
    BT_GATT_DESCRIPTOR(BT_UUID_VALID_RANGE, BT_GATT_PERM_READ, read_valid_range,
                       NULL, &co2_sensor),
    BT_GATT_CCC(co2_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    /* TVOC Concentration Characteristic */
    BT_GATT_CHARACTERISTIC(BT_UUID_TVOC_CONCENTRATION,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ, read_tvoc, NULL,
                           &tvoc_sensor.value),
    BT_GATT_DESCRIPTOR(BT_UUID_ES_MEASUREMENT, BT_GATT_PERM_READ,
                       read_es_measurement, NULL, &tvoc_sensor.meas),
    BT_GATT_CUD("TVOC Concentration", BT_GATT_PERM_READ),
    BT_GATT_DESCRIPTOR(BT_UUID_VALID_RANGE, BT_GATT_PERM_READ, read_valid_range,
                       NULL, &tvoc_sensor),
    BT_GATT_CCC(tvoc_ccc_cfg_changed,
                BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* Helper function to check if notification should be sent for 16-bit values */
static bool should_notify_16(uint8_t condition, int16_t old_val, int16_t new_val,
                             int16_t ref_val) {
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

/* Helper function to check if notification should be sent for 32-bit values */
static bool should_notify_32(uint8_t condition, int32_t old_val, int32_t new_val,
                             int32_t ref_val) {
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

/* Type-safe update functions for each sensor type */
static void update_sensor_u16(struct ess_sensor *sensor, uint16_t new_value,
                              const struct bt_gatt_attr *chrc_attr) {
  bool notify = should_notify_16(sensor->condition, (int16_t)sensor->value, (int16_t)new_value,
                                 (int16_t)sensor->ref_val);

  int32_t old_value = sensor->value;
  sensor->value = (int32_t)new_value;

  if (notify && sensor->notify_enabled) {
    uint16_t value = sys_cpu_to_le16(new_value);
    int ret = bt_gatt_notify(NULL, chrc_attr, &value, sizeof(value));
    if (ret) {
      LOG_WRN("Failed to send u16 notification: %d", ret);
    } else {
      LOG_DBG("u16 notification sent: %d -> %u", old_value, new_value);
    }
  }
}

static void update_sensor_i16(struct ess_sensor *sensor, int16_t new_value,
                              const struct bt_gatt_attr *chrc_attr) {
  bool notify = should_notify_16(sensor->condition, (int16_t)sensor->value, new_value,
                                 (int16_t)sensor->ref_val);

  int32_t old_value = sensor->value;
  sensor->value = (int32_t)new_value;

  if (notify && sensor->notify_enabled) {
    uint16_t value = sys_cpu_to_le16((uint16_t)new_value);
    int ret = bt_gatt_notify(NULL, chrc_attr, &value, sizeof(value));
    if (ret) {
      LOG_WRN("Failed to send i16 notification: %d", ret);
    } else {
      LOG_DBG("i16 notification sent: %d -> %d", old_value, new_value);
    }
  }
}

static void update_sensor_u32(struct ess_sensor *sensor, uint32_t new_value,
                              const struct bt_gatt_attr *chrc_attr) {
  bool notify = should_notify_32(sensor->condition, sensor->value, (int32_t)new_value,
                                 sensor->ref_val);

  int32_t old_value = sensor->value;
  sensor->value = (int32_t)new_value;

  if (notify && sensor->notify_enabled) {
    uint32_t value = sys_cpu_to_le32(new_value);
    int ret = bt_gatt_notify(NULL, chrc_attr, &value, sizeof(value));
    if (ret) {
      LOG_WRN("Failed to send u32 notification: %d", ret);
    } else {
      LOG_DBG("u32 notification sent: %d -> %u", old_value, new_value);
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

int ess_service_init(void) {
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

int ess_service_update(const struct sensor_data *data) {
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
  if (data->temperature_valid) {
    int16_t temp_value = convert_temperature_to_ble(data->temperature);
    update_sensor_i16(&temperature_sensor, temp_value, &ess_svc.attrs[TEMP_CHAR_ATTR_IDX]);
  }

  /* Update humidity using centralized conversion */
  if (data->humidity_valid) {
    uint16_t humid_value = convert_humidity_to_ble(data->humidity);
    update_sensor_u16(&humidity_sensor, humid_value, &ess_svc.attrs[HUMIDITY_CHAR_ATTR_IDX]);
  }

  /* Update pressure using centralized conversion */
  if (data->pressure_valid) {
    uint32_t pressure_pa = convert_pressure_to_ble(data->pressure);
    update_sensor_u32(&pressure_sensor, pressure_pa, &ess_svc.attrs[PRESSURE_CHAR_ATTR_IDX]);
  }

  /* Update CO2 concentration using centralized conversion */
  if (data->eco2_valid && sensor_manager_is_ccs811_ready()) {
    /* CCS811 is ready and data is valid - use real reading */
    uint16_t co2_value = convert_co2_to_ble(data->eco2);
    update_sensor_u16(&co2_sensor, co2_value, &ess_svc.attrs[CO2_CHAR_ATTR_IDX]);
  } else if (!sensor_manager_is_ccs811_ready()) {
    /* CCS811 is still conditioning - show 0 ppm to indicate sensor not ready */
    uint16_t co2_value = convert_co2_to_ble(0);
    update_sensor_u16(&co2_sensor, co2_value, &ess_svc.attrs[CO2_CHAR_ATTR_IDX]);
  }
  /* If sensor is ready but data invalid, keep last valid reading */

  /* Update TVOC concentration using centralized conversion */
  if (data->tvoc_valid && sensor_manager_is_ccs811_ready()) {
    /* CCS811 is ready and data is valid - use real reading */
    uint16_t tvoc_value = convert_tvoc_to_ble(data->tvoc);
    update_sensor_u16(&tvoc_sensor, tvoc_value, &ess_svc.attrs[TVOC_CHAR_ATTR_IDX]);
  } else if (!sensor_manager_is_ccs811_ready()) {
    /* CCS811 is still conditioning - show 0 ppb to indicate sensor not ready */
    uint16_t tvoc_value = convert_tvoc_to_ble(0);
    update_sensor_u16(&tvoc_sensor, tvoc_value, &ess_svc.attrs[TVOC_CHAR_ATTR_IDX]);
  }
  /* If sensor is ready but data invalid, keep last valid reading */

  /* Log current values - show conditioning status for gas sensors */
  if (data->eco2_valid && data->tvoc_valid) {
    LOG_INF("ESS updated: T=%.2f°C, H=%.1f%%, P=%.1fhPa(%.0fPa), CO2=%dppm, TVOC=%dppb",
            (double)data->temperature, (double)data->humidity,
            (double)data->pressure, (double)(data->pressure * 100), data->eco2, data->tvoc);
  } else if (sensor_manager_is_ccs811_ready()) {
    LOG_INF("ESS updated: T=%.2f°C, H=%.1f%%, P=%.1fhPa(%.0fPa), CO2/TVOC=sensor_error",
            (double)data->temperature, (double)data->humidity,
            (double)data->pressure, (double)(data->pressure * 100));
  } else {
    int64_t elapsed = k_uptime_get();
    int remaining_sec = (300000 - elapsed) / 1000; /* 5 min conditioning */
    if (remaining_sec > 0) {
      LOG_INF("ESS updated: T=%.2f°C, H=%.1f%%, P=%.1fhPa(%.0fPa), CO2/TVOC=conditioning(%ds)",
              (double)data->temperature, (double)data->humidity,
              (double)data->pressure, (double)(data->pressure * 100), remaining_sec);
    } else {
      LOG_INF("ESS updated: T=%.2f°C, H=%.1f%%, P=%.1fhPa(%.0fPa), CO2/TVOC=conditioning(finishing)",
              (double)data->temperature, (double)data->humidity,
              (double)data->pressure, (double)(data->pressure * 100));
    }
  }

  return 0;
}

int ess_service_get_temperature(void) { return temperature_sensor.value; }

int ess_service_get_humidity(void) { return humidity_sensor.value; }

int ess_service_get_pressure(void) { return pressure_sensor.value; }

int ess_service_get_co2(void) { return co2_sensor.value; }

int ess_service_get_tvoc(void) { return tvoc_sensor.value; }
