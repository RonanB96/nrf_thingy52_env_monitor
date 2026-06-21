/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <string.h>

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/ztest.h>

#include "bt_gatt.h"
#include "ess_encode.h"
#include "ess_service.h"
#include "sensor_manager.h"
#include "sensor_manager_mock.h"

LOG_MODULE_REGISTER(test_ess_service, CONFIG_LOG_DEFAULT_LEVEL);

/*
 * Tests for ess_service.c application logic. The unit-under-test installs
 * a static BT_GATT_SERVICE_DEFINE table, which we walk via the exported
 * `ess_svc` symbol to invoke read callbacks the same way the GATT layer
 * would. All sensor_manager_* and bt_gatt_* externals are FFF-faked.
 */

/* Service exported by BT_GATT_SERVICE_DEFINE(ess_svc, ...) in production. */
extern const struct bt_gatt_service_static ess_svc;

/* Per-characteristic block layout in ess_svc.attrs (each char is 6 attrs):
 *   +0 CHRC declaration  +1 VALUE  +2 ES_MEASUREMENT  +3 CUD  +4 VALID_RANGE  +5 CCC
 * Block starts come from production's ess_char_attr_idx (1, 7, 13, 19, 25),
 * which point at the CHARACTERISTIC declaration; the VALUE attr is at +1.
 */
#define ESS_VAL_TEMPERATURE 2
#define ESS_VAL_HUMIDITY    8
#define ESS_VAL_PRESSURE    14
#define ESS_VAL_CO2         20
#define ESS_VAL_TVOC        26

#define ESS_DESC_TEMP_MEAS  3  /* ES Measurement descriptor for temperature */
#define ESS_DESC_TEMP_RANGE 5  /* Valid Range descriptor for temperature */

/* Capture the buffer/value pair handed to bt_gatt_attr_read so tests can
 * assert what the service serialised on the wire.
 */
static uint8_t last_value_buf[32];
static uint16_t last_value_len;
static struct sensor_data fake_sensor_snapshot;
static int fake_sensor_get_data_ret;

static ssize_t capture_attr_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
				 uint16_t buf_len, uint16_t offset, const void *value,
				 uint16_t value_len)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(attr);
	ARG_UNUSED(offset);

	last_value_len = MIN(value_len, (uint16_t)sizeof(last_value_buf));
	if (value != NULL) {
		memcpy(last_value_buf, value, last_value_len);
	}
	if (buf != NULL && value != NULL) {
		uint16_t copy_len = MIN(buf_len, value_len);
		memcpy(buf, value, copy_len);
	}
	return (ssize_t)value_len;
}

static int copy_sensor_snapshot(struct sensor_data *data)
{
	if (fake_sensor_get_data_ret != 0) {
		return fake_sensor_get_data_ret;
	}

	if (data != NULL) {
		*data = fake_sensor_snapshot;
	}

	return 0;
}

static void before_each(void *f)
{
	ARG_UNUSED(f);
	BT_GATT_FFF_FAKES_LIST(RESET_FAKE);
	SENSOR_MANAGER_FFF_FAKES_LIST(RESET_FAKE);
	bt_gatt_attr_read_fake.custom_fake = capture_attr_read;
	sensor_manager_get_data_fake.custom_fake = copy_sensor_snapshot;
	memset(last_value_buf, 0, sizeof(last_value_buf));
	last_value_len = 0;
	memset(&fake_sensor_snapshot, 0, sizeof(fake_sensor_snapshot));
	fake_sensor_get_data_ret = 0;
}

ZTEST_SUITE(ess_service, NULL, NULL, before_each, NULL, NULL);

/* a_-prefixed tests run before init so we can exercise guards. */
ZTEST(ess_service, test_a_update_returns_enodev_before_init)
{
	struct sensor_data data = {.valid_mask = SENSOR_TEMPERATURE};
	int ret = ess_service_update(&data);

	zassert_equal(ret, -ENODEV, "Expected -ENODEV before init, got %d", ret);
}

ZTEST(ess_service, test_init_registers_sensor_callback)
{
	int ret = ess_service_init();

	zassert_equal(ret, 0, "init must succeed, got %d", ret);
	zassert_equal(sensor_manager_register_callback_fake.call_count, 1,
		      "init must register a sensor-manager callback exactly once");
	zassert_not_null(sensor_manager_register_callback_fake.arg0_val,
			 "registered callback must be non-NULL");
}

ZTEST(ess_service, test_getters_return_enodata_until_first_valid_update)
{
	zassert_equal(ess_service_init(), 0, "init prerequisite");

	zassert_equal(ess_service_get_temperature(), -ENODATA,
		      "temperature must not expose a fabricated startup value");
	zassert_equal(ess_service_get_humidity(), -ENODATA,
		      "humidity must not expose a fabricated startup value");
	zassert_equal(ess_service_get_pressure(), -ENODATA,
		      "pressure must not expose a fabricated startup value");
}

ZTEST(ess_service, test_update_null_data_returns_einval)
{
	zassert_equal(ess_service_init(), 0, "init prerequisite");

	int ret = ess_service_update(NULL);

	zassert_equal(ret, -EINVAL, "Expected -EINVAL for NULL data, got %d", ret);
}

ZTEST(ess_service, test_update_temperature_propagates_to_getter)
{
	zassert_equal(ess_service_init(), 0, "init prerequisite");

	struct sensor_data data = {
		.valid_mask = SENSOR_TEMPERATURE,
		.temperature = 21.50f,
	};
	int ret = ess_service_update(&data);

	zassert_equal(ret, 0, "update must succeed");
	/* 21.50 °C in 0.01 °C units = 2150 */
	zassert_equal(ess_service_get_temperature(), 2150,
		      "temperature getter must reflect encoded update, got %d",
		      ess_service_get_temperature());
}

ZTEST(ess_service, test_update_humidity_propagates_to_getter)
{
	zassert_equal(ess_service_init(), 0, "init prerequisite");

	struct sensor_data data = {
		.valid_mask = SENSOR_HUMIDITY,
		.humidity = 42.0f,
	};
	zassert_equal(ess_service_update(&data), 0, "update must succeed");

	/* 42.0 % in 0.01 % units = 4200 */
	zassert_equal(ess_service_get_humidity(), 4200,
		      "humidity getter mismatch, got %d", ess_service_get_humidity());
}

ZTEST(ess_service, test_update_pressure_propagates_to_getter)
{
	zassert_equal(ess_service_init(), 0, "init prerequisite");

	struct sensor_data data = {
		.valid_mask = SENSOR_PRESSURE,
		.pressure = 101.325f,
	};
	zassert_equal(ess_service_update(&data), 0, "update must succeed");

	/* 101.325 kPa = 1013.25 hPa = 1013250 in 0.1 Pa units -> ess_encode_pressure */
	int p = ess_service_get_pressure();
	zassert_within(p, 1013250, 5, "pressure getter %d not within tolerance", p);
}

ZTEST(ess_service, test_update_air_quality_when_ccs811_ready_uses_real_values)
{
	zassert_equal(ess_service_init(), 0, "init prerequisite");
	sensor_manager_is_ccs811_ready_fake.return_val = true;

	struct sensor_data data = {
		.valid_mask = SENSOR_AIR_QUALITY,
		.eco2 = 800,
		.tvoc = 250,
	};
	zassert_equal(ess_service_update(&data), 0, "update must succeed");

	zassert_equal(ess_service_get_co2(), 800, "CO2 getter must reflect raw value");
	zassert_equal(ess_service_get_tvoc(), 250, "TVOC getter must reflect raw value");
}

ZTEST(ess_service, test_update_air_quality_when_ccs811_not_ready_reports_unknown)
{
	zassert_equal(ess_service_init(), 0, "init prerequisite");
	sensor_manager_is_ccs811_ready_fake.return_val = false;

	struct sensor_data data = {
		.valid_mask = SENSOR_AIR_QUALITY,
		.eco2 = 1234, /* should be ignored */
		.tvoc = 5678,
	};
	zassert_equal(ess_service_update(&data), 0, "update must succeed");

	zassert_equal(ess_service_get_co2(), ESS_UNKNOWN_U16,
		      "CO2 must report 0xFFFF while CCS811 conditioning, got %d",
		      ess_service_get_co2());
	zassert_equal(ess_service_get_tvoc(), ESS_UNKNOWN_U16,
		      "TVOC must report 0xFFFF while CCS811 conditioning, got %d",
		      ess_service_get_tvoc());
}

ZTEST(ess_service, test_update_no_notify_when_ccc_not_subscribed)
{
	zassert_equal(ess_service_init(), 0, "init prerequisite");

	struct sensor_data data = {
		.valid_mask = SENSOR_TEMPERATURE | SENSOR_HUMIDITY | SENSOR_PRESSURE,
		.temperature = 22.0f,
		.humidity = 55.0f,
		.pressure = 101.0f,
	};
	zassert_equal(ess_service_update(&data), 0, "update must succeed");

	/* No CCC has been written so notify_enabled is false on every sensor. */
	zassert_equal(bt_gatt_notify_cb_fake.call_count, 0,
		      "no notifications expected without CCC subscribe, got %u",
		      bt_gatt_notify_cb_fake.call_count);
}

/* ---- read callback contract ---------------------------------------------- */

ZTEST(ess_service, test_read_temperature_triggers_temp_humidity_fetch)
{
	zassert_equal(ess_service_init(), 0, "init prerequisite");
	sensor_manager_update_selective_fake.return_val = 0;
	fake_sensor_snapshot.temperature = 19.75f;
	fake_sensor_snapshot.humidity = 33.0f;
	fake_sensor_snapshot.valid_mask = SENSOR_TEMP_HUMIDITY;

	const struct bt_gatt_attr *attr = &ess_svc.attrs[ESS_VAL_TEMPERATURE];
	uint8_t buf[2] = {0};

	zassert_not_null(attr->read, "temperature value attr must have read cb");
	ssize_t n = attr->read(NULL, attr, buf, sizeof(buf), 0);
	zassert_equal(n, (ssize_t)sizeof(uint16_t), "expected 2-byte read, got %zd", n);

	zassert_equal(sensor_manager_update_selective_fake.call_count, 1,
		      "must trigger one selective update");
	zassert_equal(sensor_manager_update_selective_fake.arg0_val,
		      (enum sensor_select)SENSOR_TEMP_HUMIDITY,
		      "must request TEMP+HUMIDITY together");

	/* 19.75 °C -> 1975 (0.01 °C units), little-endian on the wire. */
	int16_t got = (int16_t)sys_get_le16(buf);
	zassert_equal(got, 1975, "decoded value %d != 1975", got);
}

ZTEST(ess_service, test_read_pressure_triggers_pressure_fetch)
{
	zassert_equal(ess_service_init(), 0, "init prerequisite");
	sensor_manager_update_selective_fake.return_val = 0;
	fake_sensor_snapshot.pressure = 100.0f;
	fake_sensor_snapshot.valid_mask = SENSOR_PRESSURE;

	const struct bt_gatt_attr *attr = &ess_svc.attrs[ESS_VAL_PRESSURE];
	uint8_t buf[4] = {0};

	zassert_not_null(attr->read, "pressure value attr must have read cb");
	ssize_t n = attr->read(NULL, attr, buf, sizeof(buf), 0);
	zassert_equal(n, (ssize_t)sizeof(uint32_t), "expected 4-byte read, got %zd", n);

	zassert_equal(sensor_manager_update_selective_fake.call_count, 1,
		      "must trigger one selective update");
	zassert_equal(sensor_manager_update_selective_fake.arg0_val,
		      (enum sensor_select)SENSOR_PRESSURE,
		      "must request PRESSURE only");
}

ZTEST(ess_service, test_read_co2_when_not_ready_reports_unknown)
{
	zassert_equal(ess_service_init(), 0, "init prerequisite");
	sensor_manager_is_ccs811_ready_fake.return_val = false;

	const struct bt_gatt_attr *attr = &ess_svc.attrs[ESS_VAL_CO2];
	uint8_t buf[2] = {0};

	zassert_not_null(attr->read, "CO2 value attr must have read cb");
	ssize_t n = attr->read(NULL, attr, buf, sizeof(buf), 0);
	zassert_equal(n, (ssize_t)sizeof(uint16_t), "expected 2-byte read, got %zd", n);

	/* Wire-format assertion: the bytes a client would actually receive must
	 * be the BT SIG "value not known" sentinel 0xFFFF, little-endian. */
	uint16_t wire = sys_get_le16(buf);
	zassert_equal(wire, ESS_UNKNOWN_U16,
		      "on-the-wire CO2 must be 0xFFFF when CCS811 not ready, got 0x%04x",
		      wire);
	zassert_equal(ess_service_get_co2(), ESS_UNKNOWN_U16,
		      "CO2 getter must agree with wire format");
	zassert_equal(sensor_manager_update_selective_fake.call_count, 0,
		      "must NOT poke sensor_manager when CCS811 not ready");
}

ZTEST(ess_service, test_read_co2_when_ready_fetches_air_quality)
{
	zassert_equal(ess_service_init(), 0, "init prerequisite");
	sensor_manager_is_ccs811_ready_fake.return_val = true;
	fake_sensor_snapshot.eco2 = 1500;
	fake_sensor_snapshot.tvoc = 42;
	fake_sensor_snapshot.valid_mask = SENSOR_AIR_QUALITY;

	const struct bt_gatt_attr *attr = &ess_svc.attrs[ESS_VAL_CO2];
	uint8_t buf[2] = {0};

	(void)attr->read(NULL, attr, buf, sizeof(buf), 0);

	zassert_equal(sensor_manager_update_air_quality_for_ble_fake.call_count, 1,
		      "must use BLE air-quality refresh path");
	zassert_equal(ess_service_get_co2(), 1500, "CO2 getter mismatch");
}

ZTEST(ess_service, test_read_tvoc_when_ready_fetches_air_quality)
{
	zassert_equal(ess_service_init(), 0, "init prerequisite");
	sensor_manager_is_ccs811_ready_fake.return_val = true;
	fake_sensor_snapshot.eco2 = 800;
	fake_sensor_snapshot.tvoc = 750;
	fake_sensor_snapshot.valid_mask = SENSOR_AIR_QUALITY;

	const struct bt_gatt_attr *attr = &ess_svc.attrs[ESS_VAL_TVOC];
	uint8_t buf[2] = {0};

	(void)attr->read(NULL, attr, buf, sizeof(buf), 0);

	zassert_equal(sensor_manager_update_air_quality_for_ble_fake.call_count, 1,
		      "must use BLE air-quality refresh path");
	zassert_equal(ess_service_get_tvoc(), 750, "TVOC getter mismatch");
}

ZTEST(ess_service, test_read_es_measurement_emits_packed_descriptor)
{
	zassert_equal(ess_service_init(), 0, "init prerequisite");

	const struct bt_gatt_attr *attr = &ess_svc.attrs[ESS_DESC_TEMP_MEAS];
	uint8_t buf[16] = {0};

	zassert_not_null(attr->read, "ES Measurement descriptor must have read cb");
	ssize_t n = attr->read(NULL, attr, buf, sizeof(buf), 0);

	/* Wire layout (per BT SIG ESS spec, packed by my read_es_measurement):
	 *   [0..1]  flags        (LE u16)
	 *   [2]     sampling fn  (u8)
	 *   [3..5]  meas period  (LE u24)
	 *   [6..8]  upd interval (LE u24)
	 *   [9]     application  (u8)
	 *   [10]    uncertainty  (u8)
	 */
	zassert_equal(n, 11, "ES Measurement descriptor must be 11 bytes, got %zd", n);
	zassert_equal(last_value_len, 11, "captured value len mismatch");

	uint16_t flags = sys_get_le16(&buf[0]);
	uint8_t  sampling = buf[2];
	uint32_t meas_period = sys_get_le24(&buf[3]);
	uint32_t upd_interval = sys_get_le24(&buf[6]);
	uint8_t  application = buf[9];
	uint8_t  uncertainty = buf[10];

	/* All values come from temperature_sensor.meas in ess_service.c. */
	zassert_equal(flags, 0, "flags field must be 0 (RFU), got 0x%04x", flags);
	zassert_equal(sampling, 0x01,
		      "sampling function must be INSTANTANEOUS (0x01), got 0x%02x", sampling);
	zassert_equal(meas_period, 0U,
		      "measurement period must be 0 (on-demand), got %u", meas_period);
	zassert_equal(upd_interval, 0U,
		      "update interval must be 0 (no fixed interval), got %u", upd_interval);
	zassert_equal(application, 0x14,
		      "application must be ESS_APP_INDOOR (0x14), got 0x%02x", application);
	zassert_equal(uncertainty, 0x32,
		      "temperature uncertainty must be 0x32 (±0.50°C), got 0x%02x",
		      uncertainty);
}

ZTEST(ess_service, test_read_valid_range_u16_for_temperature)
{
	zassert_equal(ess_service_init(), 0, "init prerequisite");

	const struct bt_gatt_attr *attr = &ess_svc.attrs[ESS_DESC_TEMP_RANGE];
	uint8_t buf[8] = {0};

	zassert_not_null(attr->read, "valid range descriptor must have read cb");
	ssize_t n = attr->read(NULL, attr, buf, sizeof(buf), 0);

	/* Wire layout: lower (LE s16) | upper (LE s16) = 4 bytes. The temperature
	 * sensor's range comes from TEMP_LOWER_LIMIT/TEMP_UPPER_LIMIT in ess_service.c
	 * which alias ess_encode's constants: -27315 (-273.15°C) .. 32767. */
	zassert_equal(n, 4, "u16 valid range descriptor must be 4 bytes, got %zd", n);

	int16_t lower = (int16_t)sys_get_le16(&buf[0]);
	int16_t upper = (int16_t)sys_get_le16(&buf[2]);

	zassert_equal(lower, ESS_TEMP_MIN_E_2C,
		      "lower bound must be ESS_TEMP_MIN_E_2C (%d), got %d",
		      ESS_TEMP_MIN_E_2C, lower);
	zassert_equal(upper, ESS_TEMP_MAX_E_2C,
		      "upper bound must be ESS_TEMP_MAX_E_2C (%d), got %d",
		      ESS_TEMP_MAX_E_2C, upper);
}
