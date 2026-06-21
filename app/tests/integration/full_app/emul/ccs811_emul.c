/*
 * Copyright (c) 2026 Eaton
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Minimal CCS811 I2C emulator. Initial register values are seeded so the
 * upstream ams,ccs811 driver passes:
 *   - HW_ID  (reg 0x20) = 0x81 (CCS881_HW_ID)
 *   - STATUS (reg 0x00) = APP_VALID | FW_MODE (bits 4 and 7) so
 *     switch_to_app_mode() short-circuits on "already in app mode"
 *   - all other reads return 0 (no error bits set)
 * No real algorithm is emulated — see hts221_emul.c for rationale.
 */

#define DT_DRV_COMPAT ams_ccs811

#include <zephyr/device.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>
#include <zephyr/sys/util.h>
#include <string.h>

#define CCS811_REG_STATUS 0x00
#define CCS811_REG_HW_ID  0x20

#define CCS811_STATUS_APP_VALID BIT(4)
#define CCS811_STATUS_FW_MODE   BIT(7)
#define CCS881_HW_ID            0x81

#define CCS811_REG_COUNT 256

struct ccs811_emul_data {
	uint8_t reg[CCS811_REG_COUNT];
};

struct ccs811_emul_cfg {
};

static int ccs811_emul_init(const struct emul *target, const struct device *parent)
{
	struct ccs811_emul_data *data = target->data;

	ARG_UNUSED(parent);
	memset(data->reg, 0, sizeof(data->reg));
	data->reg[CCS811_REG_HW_ID] = CCS881_HW_ID;
	data->reg[CCS811_REG_STATUS] = CCS811_STATUS_APP_VALID | CCS811_STATUS_FW_MODE;
	return 0;
}

static int ccs811_emul_transfer(const struct emul *target, struct i2c_msg *msgs,
				int num_msgs, int addr)
{
	struct ccs811_emul_data *data = target->data;
	uint8_t reg;

	ARG_UNUSED(addr);
	if (num_msgs < 1) {
		return -EIO;
	}

	/* The CCS811 driver also issues raw multi-byte writes for the
	 * software reset sequence (no leading register address). Treat any
	 * single write whose first byte is not a known register as a no-op.
	 */
	if (msgs[0].flags & I2C_MSG_READ) {
		/* Bare read with no register select: return 0 bytes. */
		for (int i = 0; i < msgs[0].len; i++) {
			msgs[0].buf[i] = 0;
		}
		return 0;
	}
	if (msgs[0].len < 1) {
		return -EIO;
	}
	reg = msgs[0].buf[0];

	/* Reset opcode 0xFF is followed by a 4-byte unlock key, not by data
	 * destined for register 0xFF. Treat it as a software reset and
	 * re-seed the gating registers.
	 */
	if (reg == 0xFF) {
		memset(data->reg, 0, sizeof(data->reg));
		data->reg[CCS811_REG_HW_ID] = CCS881_HW_ID;
		data->reg[CCS811_REG_STATUS] =
			CCS811_STATUS_APP_VALID | CCS811_STATUS_FW_MODE;
		return 0;
	}

	if (num_msgs == 1) {
		/* Pure write: register addr + N data bytes. Auto-inc store. */
		for (int i = 1; i < msgs[0].len; i++) {
			data->reg[(uint8_t)(reg + i - 1)] = msgs[0].buf[i];
		}
		return 0;
	}

	if (msgs[1].flags & I2C_MSG_READ) {
		for (int i = 0; i < msgs[1].len; i++) {
			msgs[1].buf[i] = data->reg[(uint8_t)(reg + i)];
		}
		return 0;
	}

	for (int i = 0; i < msgs[1].len; i++) {
		data->reg[(uint8_t)(reg + i)] = msgs[1].buf[i];
	}
	return 0;
}

static const struct i2c_emul_api ccs811_emul_api = {
	.transfer = ccs811_emul_transfer,
};

#define CCS811_EMUL(n)                                                                 \
	static struct ccs811_emul_data ccs811_emul_data_##n;                           \
	static const struct ccs811_emul_cfg ccs811_emul_cfg_##n;                       \
	EMUL_DT_INST_DEFINE(n, ccs811_emul_init, &ccs811_emul_data_##n,                \
			    &ccs811_emul_cfg_##n, &ccs811_emul_api, NULL)

DT_INST_FOREACH_STATUS_OKAY(CCS811_EMUL);
