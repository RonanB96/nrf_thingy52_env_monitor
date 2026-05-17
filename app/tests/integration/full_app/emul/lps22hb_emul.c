/*
 * Copyright (c) 2026 Eaton
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Minimal LPS22HB I2C emulator. Only satisfies WHO_AM_I (reg 0x0F = 0xB1)
 * so the upstream lps22hb driver passes init. All other register accesses
 * are acked with zero data. See hts221_emul.c for rationale.
 */

#define DT_DRV_COMPAT st_lps22hb_press

#include <zephyr/device.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>
#include <zephyr/sys/util.h>
#include <string.h>

#define LPS22HB_REG_WHO_AM_I 0x0F
#define LPS22HB_ID           0xB1

#define LPS22HB_REG_COUNT 128

struct lps22hb_emul_data {
	uint8_t reg[LPS22HB_REG_COUNT];
};

struct lps22hb_emul_cfg {
};

static int lps22hb_emul_init(const struct emul *target, const struct device *parent)
{
	struct lps22hb_emul_data *data = target->data;

	ARG_UNUSED(parent);
	memset(data->reg, 0, sizeof(data->reg));
	data->reg[LPS22HB_REG_WHO_AM_I] = LPS22HB_ID;
	return 0;
}

static int lps22hb_emul_transfer(const struct emul *target, struct i2c_msg *msgs,
				 int num_msgs, int addr)
{
	struct lps22hb_emul_data *data = target->data;
	uint8_t reg;

	ARG_UNUSED(addr);
	if (num_msgs < 1 || (msgs[0].flags & I2C_MSG_READ) || msgs[0].len < 1) {
		return -EIO;
	}
	reg = msgs[0].buf[0] & 0x7F;

	if (num_msgs == 1) {
		for (int i = 1; i < msgs[0].len; i++) {
			if (reg < LPS22HB_REG_COUNT) {
				data->reg[reg++] = msgs[0].buf[i];
			}
		}
		return 0;
	}

	if (msgs[1].flags & I2C_MSG_READ) {
		for (int i = 0; i < msgs[1].len; i++) {
			msgs[1].buf[i] = (reg < LPS22HB_REG_COUNT) ? data->reg[reg] : 0;
			reg++;
		}
		return 0;
	}

	for (int i = 0; i < msgs[1].len; i++) {
		if (reg < LPS22HB_REG_COUNT) {
			data->reg[reg++] = msgs[1].buf[i];
		}
	}
	return 0;
}

static const struct i2c_emul_api lps22hb_emul_api = {
	.transfer = lps22hb_emul_transfer,
};

#define LPS22HB_EMUL(n)                                                                \
	static struct lps22hb_emul_data lps22hb_emul_data_##n;                         \
	static const struct lps22hb_emul_cfg lps22hb_emul_cfg_##n;                     \
	EMUL_DT_INST_DEFINE(n, lps22hb_emul_init, &lps22hb_emul_data_##n,              \
			    &lps22hb_emul_cfg_##n, &lps22hb_emul_api, NULL)

DT_INST_FOREACH_STATUS_OKAY(LPS22HB_EMUL);
