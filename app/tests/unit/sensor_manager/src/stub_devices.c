/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Stub `struct device` instances for the three sensor DT nodes so
 * DEVICE_DT_GET(DT_NODELABEL(hts221|lps22hb_press|ccs811)) resolves at
 * link time on native_sim.
 *
 * Each stub init returns 0, so device_is_ready() returns true and MY
 * sensor_manager_init() takes the success path through every driver init
 * call (which is then served by the FFF fakes in mock_drivers.c).
 */

#include <zephyr/device.h>
#include <zephyr/init.h>

static int stub_dev_init(const struct device *dev)
{
	ARG_UNUSED(dev);
	return 0;
}

DEVICE_DT_DEFINE(DT_NODELABEL(hts221), stub_dev_init, NULL, NULL, NULL,
		 POST_KERNEL, 90, NULL);

DEVICE_DT_DEFINE(DT_NODELABEL(lps22hb_press), stub_dev_init, NULL, NULL, NULL,
		 POST_KERNEL, 90, NULL);

DEVICE_DT_DEFINE(DT_NODELABEL(ccs811), stub_dev_init, NULL, NULL, NULL,
		 POST_KERNEL, 90, NULL);
