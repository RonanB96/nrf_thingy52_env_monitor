/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "mock_hwinfo.h"

DEFINE_FFF_GLOBALS;

DEFINE_FAKE_VALUE_FUNC(ssize_t, z_impl_hwinfo_get_device_id, uint8_t *, size_t);
