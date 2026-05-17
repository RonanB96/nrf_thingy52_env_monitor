/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef MOCK_HWINFO_H_
#define MOCK_HWINFO_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <zephyr/fff.h>

#define HWINFO_FFF_FAKES_LIST(FAKE) FAKE(z_impl_hwinfo_get_device_id)

DECLARE_FAKE_VALUE_FUNC(ssize_t, z_impl_hwinfo_get_device_id, uint8_t *, size_t);

#endif /* MOCK_HWINFO_H_ */
