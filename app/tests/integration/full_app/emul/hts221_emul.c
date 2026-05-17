/*
 * Copyright (c) 2026 Eaton
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Minimal HTS221 I2C emulator for native_sim coverage tests.
 *
 * This emulator does NOT implement the HTS221 register file. It only
 * answers enough I2C traffic for the upstream Zephyr HTS221 driver to:
 *   - pass its WHO_AM_I check at init (HTS221_ID = 0xBC at reg 0x0F)
 *   - accept any subsequent register writes
 *   - return zero bytes for any other register read
 *
 * Calibration register reads return zero, so converted humidity/temperature
 * values are not physically meaningful. The goal here is COVERAGE of the
 * thin app/src wrapper that calls device_is_ready(), sensor_sample_fetch(),
 * and sensor_channel_get(). Functional correctness of the underlying
 * upstream driver is exercised by the HIL test suite on real hardware.
 */

#define DT_DRV_COMPAT st_hts221

#include <zephyr/device.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>
#include <zephyr/sys/util.h>
#include <string.h>

#define HTS221_REG_WHO_AM_I 0x0F
#define HTS221_ID           0xBC

#define HTS221_REG_COUNT 128

struct hts221_emul_data {
	uint8_t reg[HTS221_REG_COUNT];
};

struct hts221_emul_cfg {
};

static int hts221_emul_init(const struct emul *target, const struct device *parent)
{
	struct hts221_emul_data *data = target->data;

	ARG_UNUSED(parent);
	memset(data->reg, 0, sizeof(data->reg));
	data->reg[HTS221_REG_WHO_AM_I] = HTS221_ID;
	return 0;
}

static int hts221_emul_transfer(const struct emul *target, struct i2c_msg *msgs,
				int num_msgs, int addr)
{
	struct hts221_emul_data *data = target->data;
	uint8_t reg;

	ARG_UNUSED(addr);
	if (num_msgs < 1) {
		return -EIO;
	}

	/* First message is always a register-address write (1 byte). The ST
	 * stmemsc framework sets MSB=1 for multi-byte auto-increment reads.
	 */
	if ((msgs[0].flags & I2C_MSG_READ) || msgs[0].len < 1) {
		return -EIO;
	}
	reg = msgs[0].buf[0] & 0x7F;

	if (num_msgs == 1) {
		/* Pure write: register addr + N data bytes. Store with auto-inc. */
		for (int i = 1; i < msgs[0].len; i++) {
			if (reg < HTS221_REG_COUNT) {
				data->reg[reg++] = msgs[0].buf[i];
			}
		}
		return 0;
	}

	/* num_msgs == 2: write reg addr, then read N bytes with auto-inc. */
	if (msgs[1].flags & I2C_MSG_READ) {
		for (int i = 0; i < msgs[1].len; i++) {
			msgs[1].buf[i] = (reg < HTS221_REG_COUNT) ? data->reg[reg] : 0;
			reg++;
		}
		return 0;
	}

	/* Repeated start with two writes: store data into auto-inc registers. */
	for (int i = 0; i < msgs[1].len; i++) {
		if (reg < HTS221_REG_COUNT) {
			data->reg[reg++] = msgs[1].buf[i];
		}
	}
	return 0;
}

static const struct i2c_emul_api hts221_emul_api = {
	.transfer = hts221_emul_transfer,
};

#define HTS221_EMUL(n)                                                                 \
	static struct hts221_emul_data hts221_emul_data_##n;                           \
	static const struct hts221_emul_cfg hts221_emul_cfg_##n;                       \
	EMUL_DT_INST_DEFINE(n, hts221_emul_init, &hts221_emul_data_##n,                \
			    &hts221_emul_cfg_##n, &hts221_emul_api, NULL)

DT_INST_FOREACH_STATUS_OKAY(HTS221_EMUL);
