/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include "ble_battery_service.h"
#include "board.h"
#include "ess_encode.h"
#include "ess_service.h"
#include "sensor_ccs811_driver.h"
#include "sensor_manager.h"
#include "uptime_service.h"

struct ess_char_name {
	const char *name;
	enum ess_char_id id;
};

static const struct ess_char_name ess_char_names[] = {
	{"temp", ESS_CHAR_TEMPERATURE},  {"humidity", ESS_CHAR_HUMIDITY},
	{"pressure", ESS_CHAR_PRESSURE}, {"co2", ESS_CHAR_CO2},
	{"tvoc", ESS_CHAR_TVOC},
};

static int parse_ess_char(const char *name, enum ess_char_id *id)
{
	size_t i;

	if (name == NULL || id == NULL) {
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(ess_char_names); i++) {
		if (strcmp(name, ess_char_names[i].name) == 0) {
			*id = ess_char_names[i].id;
			return 0;
		}
	}

	return -ENOENT;
}

static const char *ess_char_label(enum ess_char_id id)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(ess_char_names); i++) {
		if (ess_char_names[i].id == id) {
			return ess_char_names[i].name;
		}
	}

	return "?";
}

static void print_hex(const struct shell *sh, const uint8_t *buf, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) {
		shell_fprintf(sh, SHELL_NORMAL, "%02x%s", buf[i], (i + 1U < len) ? " " : "");
	}
}

static void print_ess_decoded(const struct shell *sh, enum ess_char_id id, const uint8_t *buf,
			      size_t len)
{
	switch (id) {
	case ESS_CHAR_TEMPERATURE:
		if (len >= sizeof(uint16_t)) {
			int16_t raw = (int16_t)sys_get_le16(buf);

			shell_print(sh, "  decoded: %d (%.2f C)", raw, (double)raw / 100.0);
		}
		break;
	case ESS_CHAR_HUMIDITY:
		if (len >= sizeof(uint16_t)) {
			uint16_t raw = sys_get_le16(buf);

			shell_print(sh, "  decoded: %u (%.2f %%)", raw, (double)raw / 100.0);
		}
		break;
	case ESS_CHAR_PRESSURE:
		if (len >= sizeof(uint32_t)) {
			uint32_t raw = sys_get_le32(buf);

			shell_print(sh, "  decoded: %u (%.2f hPa)", raw, (double)raw / 1000.0);
		}
		break;
	case ESS_CHAR_CO2:
		if (len >= sizeof(uint16_t)) {
			uint16_t raw = sys_get_le16(buf);

			if (raw == ESS_UNKNOWN_U16) {
				shell_print(sh, "  decoded: 0xFFFF (unknown)");
			} else {
				shell_print(sh, "  decoded: %u ppm", raw);
			}
		}
		break;
	case ESS_CHAR_TVOC:
		if (len >= sizeof(uint16_t)) {
			uint16_t raw = sys_get_le16(buf);

			if (raw == ESS_UNKNOWN_U16) {
				shell_print(sh, "  decoded: 0xFFFF (unknown)");
			} else {
				shell_print(sh, "  decoded: %u ppb", raw);
			}
		}
		break;
	default:
		break;
	}
}

static int read_ess_char(const struct shell *sh, enum ess_char_id id)
{
	uint8_t buf[8] = {0};
	ssize_t n;

	n = ess_service_local_read(id, buf, sizeof(buf));
	if (n < 0) {
		shell_error(sh, "%s GATT read failed: %d", ess_char_label(id), (int)n);
		return (int)n;
	}

	shell_fprintf(sh, SHELL_NORMAL, "%s wire [%zd]: ", ess_char_label(id), n);
	print_hex(sh, buf, (size_t)n);
	shell_print(sh, "");
	print_ess_decoded(sh, id, buf, (size_t)n);
	return 0;
}

static int read_uptime(const struct shell *sh)
{
	uint8_t buf[sizeof(uint64_t)] = {0};
	ssize_t n;
	uint64_t seconds = 0;

	n = uptime_service_local_read(buf, sizeof(buf));
	if (n < 0) {
		shell_error(sh, "uptime GATT read failed: %d", (int)n);
		return (int)n;
	}

	memcpy(&seconds, buf, sizeof(seconds));
	seconds = sys_le64_to_cpu(seconds);
	shell_fprintf(sh, SHELL_NORMAL, "uptime wire [%zd]: ", n);
	print_hex(sh, buf, (size_t)n);
	shell_print(sh, "");
	shell_print(sh, "  decoded: %llu s", (unsigned long long)seconds);
	return 0;
}

static int read_battery(const struct shell *sh)
{
	/* Zephyr BAS owns the GATT read; print the app cache a central would see. */
	shell_print(sh, "battery cache: %u%% charging=%s", ble_battery_service_get_level(),
		    ble_battery_service_is_charging() ? "yes" : "no");
	return 0;
}

static int cmd_gatt_connect(const struct shell *sh, size_t argc, char **argv)
{
	int ret;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	ret = sensor_manager_on_connected();
	if (ret != 0) {
		shell_error(sh, "sensor_manager_on_connected: %d", ret);
		return ret;
	}

	ret = ble_battery_service_on_connected();
	if (ret != 0) {
		shell_error(sh, "ble_battery_service_on_connected: %d", ret);
		return ret;
	}

	shell_print(sh, "simulated GATT connect (count=%u)", sensor_manager_get_connection_count());
	return 0;
}

static int cmd_gatt_disconnect(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	sensor_manager_on_disconnected();
	ble_battery_service_on_disconnected();
	shell_print(sh, "simulated GATT disconnect (count=%u)",
		    sensor_manager_get_connection_count());
	return 0;
}

static int cmd_gatt_read(const struct shell *sh, size_t argc, char **argv)
{
	enum ess_char_id id;
	int ret;

	if (argc < 2) {
		shell_error(
			sh,
			"usage: app gatt read <temp|humidity|pressure|co2|tvoc|uptime|battery>");
		return -EINVAL;
	}

	if (strcmp(argv[1], "uptime") == 0) {
		return read_uptime(sh);
	}

	if (strcmp(argv[1], "battery") == 0) {
		return read_battery(sh);
	}

	ret = parse_ess_char(argv[1], &id);
	if (ret != 0) {
		shell_error(sh, "unknown characteristic '%s'", argv[1]);
		return ret;
	}

	return read_ess_char(sh, id);
}

static int cmd_gatt_ccc(const struct shell *sh, size_t argc, char **argv)
{
	enum ess_char_id id;
	bool notify;
	int ret;

	if (argc < 3) {
		shell_error(sh, "usage: app gatt ccc <temp|humidity|pressure|co2|tvoc> <on|off>");
		return -EINVAL;
	}

	ret = parse_ess_char(argv[1], &id);
	if (ret != 0) {
		shell_error(sh, "unknown characteristic '%s' (battery/uptime have no CCC)",
			    argv[1]);
		return ret;
	}

	if (strcmp(argv[2], "on") == 0) {
		notify = true;
	} else if (strcmp(argv[2], "off") == 0) {
		notify = false;
	} else {
		shell_error(sh, "expected on|off, got '%s'", argv[2]);
		return -EINVAL;
	}

	ret = ess_service_local_ccc(id, notify);
	if (ret != 0) {
		shell_error(sh, "CCC set failed: %d", ret);
		return ret;
	}

	shell_print(sh, "%s CCC notify %s", ess_char_label(id), notify ? "on" : "off");
	return 0;
}

static int poll_once(const struct shell *sh)
{
	static const enum ess_char_id poll_order[] = {
		ESS_CHAR_TEMPERATURE,
		ESS_CHAR_HUMIDITY,
		ESS_CHAR_PRESSURE,
	};
	size_t i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(poll_order); i++) {
		ret = read_ess_char(sh, poll_order[i]);
		if (ret != 0) {
			return ret;
		}
	}

	ret = read_battery(sh);
	if (ret != 0) {
		return ret;
	}

	ret = read_ess_char(sh, ESS_CHAR_CO2);
	if (ret != 0) {
		return ret;
	}

	return read_ess_char(sh, ESS_CHAR_TVOC);
}

static int cmd_gatt_poll(const struct shell *sh, size_t argc, char **argv)
{
	unsigned long rounds = 1;
	unsigned long interval_ms = 0;
	unsigned long i;
	int ret;

	if (argc >= 2) {
		rounds = strtoul(argv[1], NULL, 10);
		if (rounds == 0U) {
			shell_error(sh, "poll count must be >= 1");
			return -EINVAL;
		}
	}

	if (argc >= 3) {
		interval_ms = strtoul(argv[2], NULL, 10);
	}

	for (i = 0; i < rounds; i++) {
		shell_print(sh, "--- poll %lu/%lu ---", i + 1UL, rounds);
		ret = poll_once(sh);
		if (ret != 0) {
			return ret;
		}
		if ((i + 1UL) < rounds && interval_ms > 0U) {
			int32_t sleep_ms = (interval_ms > (unsigned long)INT32_MAX)
						   ? INT32_MAX
						   : (int32_t)interval_ms;

			k_msleep(sleep_ms);
		}
	}

	return 0;
}

static int cmd_sm_status(const struct shell *sh, size_t argc, char **argv)
{
	struct sensor_data data;
	int ret;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "armed=%s count=%u", sensor_manager_is_armed() ? "yes" : "no",
		    sensor_manager_get_connection_count());

	ret = sensor_manager_get_data(&data);
	if (ret != 0) {
		shell_error(sh, "sensor_manager_get_data: %d", ret);
		return ret;
	}

	shell_print(sh, "valid_mask=0x%02x ts=%lld", (unsigned int)data.valid_mask,
		    (long long)data.timestamp);
	shell_print(sh, "T=%.2f C H=%.2f %% P=%.3f kPa CO2=%u ppm TVOC=%u ppb bat=%u%% chg=%s",
		    (double)data.temperature, (double)data.humidity, (double)data.pressure,
		    data.eco2, data.tvoc, data.battery_level, data.battery_charging ? "yes" : "no");
	return 0;
}

static int cmd_ccs811_status(const struct shell *sh, size_t argc, char **argv)
{
	struct ccs811_debug_state st;
	int ret;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	ret = ccs811_driver_get_debug_state(&st);
	if (ret != 0) {
		shell_error(sh, "ccs811_driver_get_debug_state: %d", ret);
		return ret;
	}

	shell_print(sh,
		    "ready=%s enabled=%s idle=%s ble_connected=%s await_1s=%s mode=%d cond_ms=%lld",
		    st.ready ? "yes" : "no", st.enabled ? "yes" : "no", st.idle ? "yes" : "no",
		    st.ble_connected ? "yes" : "no", st.await_first_1s_sample ? "yes" : "no",
		    st.mode, (long long)st.conditioning_remaining_ms);
	return 0;
}

static int cmd_ess_status(const struct shell *sh, size_t argc, char **argv)
{
	enum ess_char_id id;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	for (id = ESS_CHAR_TEMPERATURE; id < ESS_CHAR_COUNT; id++) {
		struct ess_char_status st;
		int ret = ess_service_get_char_status(id, &st);

		if (ret != 0) {
			shell_error(sh, "%s status: %d", ess_char_label(id), ret);
			return ret;
		}

		shell_print(sh, "%s known=%s notify=%s value=%d", ess_char_label(id),
			    st.value_known ? "yes" : "no", st.notify_enabled ? "yes" : "no",
			    st.value);
	}

	return 0;
}

static int cmd_board_pins(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(sh);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	board_print_pin_states();
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_gatt,
	SHELL_CMD_ARG(connect, NULL, "Simulate GATT connect (same app path as BLE)",
		      cmd_gatt_connect, 1, 0),
	SHELL_CMD_ARG(disconnect, NULL, "Simulate GATT disconnect", cmd_gatt_disconnect, 1, 0),
	SHELL_CMD_ARG(read, NULL, "<temp|humidity|pressure|co2|tvoc|uptime|battery>", cmd_gatt_read,
		      2, 0),
	SHELL_CMD_ARG(ccc, NULL, "<temp|humidity|pressure|co2|tvoc> <on|off>", cmd_gatt_ccc, 3, 0),
	SHELL_CMD_ARG(poll, NULL,
		      "[count] [interval_ms]  temp, humidity, pressure, battery, co2, tvoc",
		      cmd_gatt_poll, 1, 2),
	SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_sm,
			       SHELL_CMD(status, NULL, "Sensor manager state", cmd_sm_status),
			       SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_ccs811,
			       SHELL_CMD(status, NULL, "CCS811 mode/connect state",
					 cmd_ccs811_status),
			       SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_ess,
			       SHELL_CMD(status, NULL, "ESS cache and CCC flags", cmd_ess_status),
			       SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_board,
			       SHELL_CMD(pins, NULL, "Dump GPIO pin states", cmd_board_pins),
			       SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_app, SHELL_CMD(gatt, &sub_gatt, "GATT-path simulation", NULL),
			       SHELL_CMD(sm, &sub_sm, "Sensor manager", NULL),
			       SHELL_CMD(ccs811, &sub_ccs811, "CCS811 driver", NULL),
			       SHELL_CMD(ess, &sub_ess, "ESS service", NULL),
			       SHELL_CMD(board, &sub_board, "Board GPIO", NULL),
			       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(app, &sub_app, "GATT-path debug shell", NULL);
