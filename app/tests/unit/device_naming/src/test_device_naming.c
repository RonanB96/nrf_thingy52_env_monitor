/*
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Tests for app/src/device_naming.c.
 *
 * hwinfo_get_device_id() is a Zephyr syscall; CONFIG_USERSPACE=n collapses it
 * to a direct call to z_impl_hwinfo_get_device_id, which we fake via FFF.
 *
 * device_naming keeps a static `naming_initialized` cache that cannot be
 * reset between tests. The argument-validation and hwinfo-failure paths
 * never set the cache, so they are safe in any order. The success-then-cache
 * lifecycle is exercised in a single test to avoid relying on ZTEST execution
 * order, which is not part of the ztest contract.
 */

#include <errno.h>
#include <string.h>
#include <zephyr/ztest.h>
#include "device_naming.h"
#include "mock_hwinfo.h"

/* Custom fake: write deterministic 8-byte ID, return id_len. */
static ssize_t fake_get_id_8(uint8_t *buf, size_t len)
{
	const uint8_t id[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0xD4, 0xC3, 0xB2, 0xA1};
	size_t n = (len < sizeof(id)) ? len : sizeof(id);

	memcpy(buf, id, n);
	return (ssize_t)n;
}

static void before_each(void *f)
{
	ARG_UNUSED(f);
	HWINFO_FFF_FAKES_LIST(RESET_FAKE);
}

ZTEST_SUITE(device_naming, NULL, NULL, before_each, NULL, NULL);

/* ---- Argument validation: never touches the cache. ---- */

ZTEST(device_naming, test_null_buffer_returns_einval)
{
	int ret = device_naming_get_name(NULL, 64);

	zassert_equal(ret, -EINVAL, "Expected -EINVAL for NULL buffer, got %d", ret);
	zassert_equal(z_impl_hwinfo_get_device_id_fake.call_count, 0, "hwinfo must not be called");
}

ZTEST(device_naming, test_buffer_too_small_returns_einval)
{
	char buf[8]; /* < DEVICE_NAME_MIN_BUF */
	int ret = device_naming_get_name(buf, sizeof(buf));

	zassert_equal(ret, -EINVAL, "Expected -EINVAL for small buffer, got %d", ret);
	zassert_equal(z_impl_hwinfo_get_device_id_fake.call_count, 0, "hwinfo must not be called");
}

/* ---- hwinfo failure: cache stays clear because the function returns < 0. ---- */

ZTEST(device_naming, test_hwinfo_failure_returns_enodev_with_unknown_suffix)
{
	char buf[DEVICE_NAME_MAX_LEN] = {0};

	z_impl_hwinfo_get_device_id_fake.return_val = -ENOTSUP;

	int ret = device_naming_get_name(buf, sizeof(buf));

	zassert_equal(ret, -ENODEV, "Expected -ENODEV when hwinfo fails, got %d", ret);
	zassert_str_equal(buf, DEVICE_PREFIX "-UNKNOWN", "Fallback name mismatch: %s", buf);
}

ZTEST(device_naming, test_hwinfo_zero_bytes_returns_enodev)
{
	char buf[DEVICE_NAME_MAX_LEN] = {0};

	z_impl_hwinfo_get_device_id_fake.return_val = 0;

	int ret = device_naming_get_name(buf, sizeof(buf));

	zassert_equal(ret, -ENODEV, "Expected -ENODEV when hwinfo returns 0, got %d", ret);
	zassert_str_equal(buf, DEVICE_PREFIX "-UNKNOWN", "Fallback name mismatch: %s", buf);
}

/*
 * Exercises device_naming_init() error tail when the underlying hwinfo
 * read fails. Must run before the success-lifecycle test below because
 * naming_initialized would otherwise short-circuit init() to the cache.
 * The hwinfo failure path returns < 0 without setting the cache, so this
 * test does not contaminate later runs.
 */
ZTEST(device_naming, test_init_propagates_hwinfo_failure)
{
	z_impl_hwinfo_get_device_id_fake.return_val = -ENOTSUP;

	int ret = device_naming_init();

	zassert_equal(ret, -ENODEV, "init must propagate hwinfo failure as -ENODEV, got %d",
		      ret);
	zassert_equal(z_impl_hwinfo_get_device_id_fake.call_count, 1,
		      "init must call hwinfo exactly once when uncached");
}

/*
 * Full success + cache lifecycle in one test. This is the only path that
 * mutates the static cache, so keeping it self-contained removes the need to
 * rely on inter-test ordering.
 *
 * Drives device_naming_init() first so the uncached init path (delegation
 * into device_naming_get_name + post-success log) is exercised; subsequent
 * calls then verify the cached short-circuit in both entry points.
 */
ZTEST(device_naming, test_success_lifecycle_packs_id_then_caches)
{
	char buf1[DEVICE_NAME_MAX_LEN] = {0};
	char buf2[DEVICE_NAME_MAX_LEN] = {0};

	z_impl_hwinfo_get_device_id_fake.custom_fake = fake_get_id_8;

	/* First call goes through device_naming_init() so we cover the
	 * uncached branch (naming_initialized == false) and the success
	 * tail of init(). */
	int ret = device_naming_init();

	zassert_equal(ret, 0, "First init must succeed, got %d", ret);
	zassert_equal(z_impl_hwinfo_get_device_id_fake.call_count, 1,
		      "First init must read hwinfo");

	/* Verify the cached name is correct via get_name(). */
	ret = device_naming_get_name(buf1, sizeof(buf1));
	zassert_equal(ret, 0, "get_name after init must succeed, got %d", ret);
	/*
	 * id = AA BB CC DD D4 C3 B2 A1 (id_len = 8). Loop in production code
	 * packs the last 4 bytes LSB-first:
	 *   id[7]=A1<<0 | id[6]=B2<<8 | id[5]=C3<<16 | id[4]=D4<<24 = 0xD4C3B2A1
	 */
	zassert_str_equal(buf1, DEVICE_PREFIX "-D4C3B2A1", "Suffix mismatch: %s", buf1);
	zassert_equal(z_impl_hwinfo_get_device_id_fake.call_count, 1,
		      "get_name after init must hit cache (call_count=%u)",
		      z_impl_hwinfo_get_device_id_fake.call_count);

	/* Second get_name must also hit the cache. */
	ret = device_naming_get_name(buf2, sizeof(buf2));

	zassert_equal(ret, 0, "Cached call must succeed, got %d", ret);
	zassert_equal(z_impl_hwinfo_get_device_id_fake.call_count, 1,
		      "Cache must short-circuit hwinfo (call_count=%u)",
		      z_impl_hwinfo_get_device_id_fake.call_count);
	zassert_str_equal(buf2, buf1, "Cached value mismatch: %s vs %s", buf2, buf1);

	/* device_naming_init() also takes the cached path. */
	ret = device_naming_init();
	zassert_equal(ret, 0, "Cached init must succeed, got %d", ret);
	zassert_equal(z_impl_hwinfo_get_device_id_fake.call_count, 1,
		      "init() must not re-read hwinfo when cache is valid");
}
