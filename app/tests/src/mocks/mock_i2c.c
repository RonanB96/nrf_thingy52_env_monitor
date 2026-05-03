/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/ztest.h>
#include <zephyr/logging/log.h>
#include "mock_i2c.h"

LOG_MODULE_REGISTER(mock_i2c, CONFIG_LOG_DEFAULT_LEVEL);

/* Mock I2C state */
#define MAX_EXPECTED_TRANSACTIONS 32

static struct {
	struct mock_i2c_transaction expected[MAX_EXPECTED_TRANSACTIONS];
	int expected_count;
	int current_index;
	bool device_ready_states[8]; /* Support up to 8 mock devices */
	int device_count;
} mock_i2c_state;

void mock_i2c_init(void)
{
	LOG_DBG("Initializing mock I2C system");
	memset(&mock_i2c_state, 0, sizeof(mock_i2c_state));

	/* Set all devices ready by default */
	for (int i = 0; i < ARRAY_SIZE(mock_i2c_state.device_ready_states); i++) {
		mock_i2c_state.device_ready_states[i] = true;
	}
}

void mock_i2c_reset(void)
{
	LOG_DBG("Resetting mock I2C state");
	mock_i2c_state.expected_count = 0;
	mock_i2c_state.current_index = 0;
}

void mock_i2c_expect_transaction(const struct mock_i2c_transaction *transaction)
{
	zassert_true(mock_i2c_state.expected_count < MAX_EXPECTED_TRANSACTIONS,
		     "Too many expected I2C transactions");

	mock_i2c_state.expected[mock_i2c_state.expected_count] = *transaction;
	mock_i2c_state.expected_count++;

	LOG_DBG("Added expected I2C transaction %d: type=%d, addr=0x%04x",
		mock_i2c_state.expected_count - 1, transaction->type, transaction->addr);
}

void mock_i2c_set_device_ready(const struct device *dev, bool ready)
{
	/* For now, just use a simple index based on device pointer */
	int device_index = ((uintptr_t)dev) % ARRAY_SIZE(mock_i2c_state.device_ready_states);
	mock_i2c_state.device_ready_states[device_index] = ready;

	LOG_DBG("Set mock I2C device %p ready state to %s", dev, ready ? "true" : "false");
}

void mock_i2c_verify_complete(void)
{
	zassert_equal(mock_i2c_state.current_index, mock_i2c_state.expected_count,
		      "Not all expected I2C transactions were executed. "
		      "Expected: %d, Actual: %d",
		      mock_i2c_state.expected_count, mock_i2c_state.current_index);

	LOG_INF("All %d expected I2C transactions completed successfully",
		mock_i2c_state.expected_count);
}

/* Internal helper to process mock transactions */
static int process_mock_transaction(const struct mock_i2c_transaction *actual)
{
	if (mock_i2c_state.current_index >= mock_i2c_state.expected_count) {
		LOG_ERR("Unexpected I2C transaction: type=%d, addr=0x%04x", actual->type,
			actual->addr);
		zassert_true(false, "Unexpected I2C transaction");
		return -EIO;
	}

	const struct mock_i2c_transaction *expected =
		&mock_i2c_state.expected[mock_i2c_state.current_index];

	/* Verify transaction matches expectation */
	zassert_equal(actual->type, expected->type, "I2C transaction type mismatch at index %d",
		      mock_i2c_state.current_index);
	zassert_equal(actual->addr, expected->addr, "I2C address mismatch at index %d",
		      mock_i2c_state.current_index);

	if (expected->write_len > 0) {
		zassert_equal(actual->write_len, expected->write_len,
			      "I2C write length mismatch at index %d",
			      mock_i2c_state.current_index);
		zassert_mem_equal(actual->write_buf, expected->write_buf, expected->write_len,
				  "I2C write data mismatch at index %d",
				  mock_i2c_state.current_index);
	}

	if (expected->read_len > 0 && actual->read_buf != NULL) {
		zassert_equal(actual->read_len, expected->read_len,
			      "I2C read length mismatch at index %d", mock_i2c_state.current_index);

		/* Copy expected read data to actual buffer */
		if (expected->read_buf != NULL) {
			memcpy(actual->read_buf, expected->read_buf, expected->read_len);
		}
	}

	mock_i2c_state.current_index++;

	LOG_DBG("Processed I2C transaction %d successfully", mock_i2c_state.current_index - 1);

	return expected->expected_return;
}

/* Mock implementations for when I2C driver is disabled */
#ifndef CONFIG_I2C

/* Override device_is_ready for I2C devices */
bool device_is_ready_mock(const struct device *dev)
{
	int device_index = ((uintptr_t)dev) % ARRAY_SIZE(mock_i2c_state.device_ready_states);
	bool ready = mock_i2c_state.device_ready_states[device_index];

	LOG_DBG("Mock device_is_ready(%p) = %s", dev, ready ? "true" : "false");
	return ready;
}

int i2c_write_mock(const struct device *dev, const uint8_t *buf, uint32_t num_bytes, uint16_t addr)
{
	LOG_DBG("Mock I2C write: dev=%p, addr=0x%04x, len=%d", dev, addr, num_bytes);

	/* Check device readiness first */
	int device_index = ((uintptr_t)dev) % ARRAY_SIZE(mock_i2c_state.device_ready_states);
	if (!mock_i2c_state.device_ready_states[device_index]) {
		LOG_WRN("I2C device not ready");
		return -ENODEV;
	}

	struct mock_i2c_transaction transaction = {.type = MOCK_I2C_WRITE,
						   .addr = addr,
						   .write_buf = buf,
						   .write_len = num_bytes,
						   .read_buf = NULL,
						   .read_len = 0,
						   .expected_return = 0};

	return process_mock_transaction(&transaction);
}

int i2c_read_mock(const struct device *dev, uint8_t *buf, uint32_t num_bytes, uint16_t addr)
{
	LOG_DBG("Mock I2C read: dev=%p, addr=0x%04x, len=%d", dev, addr, num_bytes);

	/* Check device readiness first */
	int device_index = ((uintptr_t)dev) % ARRAY_SIZE(mock_i2c_state.device_ready_states);
	if (!mock_i2c_state.device_ready_states[device_index]) {
		LOG_WRN("I2C device not ready");
		return -ENODEV;
	}

	struct mock_i2c_transaction transaction = {.type = MOCK_I2C_READ,
						   .addr = addr,
						   .write_buf = NULL,
						   .write_len = 0,
						   .read_buf = buf,
						   .read_len = num_bytes,
						   .expected_return = 0};

	return process_mock_transaction(&transaction);
}

int i2c_write_read_mock(const struct device *dev, uint16_t addr, const void *write_buf,
			size_t num_write, void *read_buf, size_t num_read)
{
	LOG_DBG("Mock I2C write_read: dev=%p, addr=0x%04x, write_len=%zu, read_len=%zu", dev, addr,
		num_write, num_read);

	/* Check device readiness first */
	int device_index = ((uintptr_t)dev) % ARRAY_SIZE(mock_i2c_state.device_ready_states);
	if (!mock_i2c_state.device_ready_states[device_index]) {
		LOG_WRN("I2C device not ready");
		return -ENODEV;
	}

	struct mock_i2c_transaction transaction = {.type = MOCK_I2C_WRITE_READ,
						   .addr = addr,
						   .write_buf = (const uint8_t *)write_buf,
						   .write_len = num_write,
						   .read_buf = (uint8_t *)read_buf,
						   .read_len = num_read,
						   .expected_return = 0};

	return process_mock_transaction(&transaction);
}

#endif /* !CONFIG_I2C */