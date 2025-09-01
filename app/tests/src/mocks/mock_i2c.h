/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef MOCK_I2C_H
#define MOCK_I2C_H

#include <zephyr/ztest.h>
#include <zephyr/drivers/i2c.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mock I2C interface for testing
 * 
 * This mock implementation replaces the Zephyr I2C driver for unit testing,
 * allowing tests to simulate I2C transactions without hardware dependencies.
 */

/* Mock I2C transaction types */
enum mock_i2c_transaction_type {
    MOCK_I2C_WRITE,
    MOCK_I2C_READ,
    MOCK_I2C_WRITE_READ
};

/* Mock I2C transaction data */
struct mock_i2c_transaction {
    enum mock_i2c_transaction_type type;
    uint16_t addr;
    const uint8_t *write_buf;
    size_t write_len;
    uint8_t *read_buf;
    size_t read_len;
    int expected_return;
};

/**
 * @brief Initialize mock I2C system
 */
void mock_i2c_init(void);

/**
 * @brief Reset mock I2C state
 */
void mock_i2c_reset(void);

/**
 * @brief Add expected I2C transaction
 * 
 * @param transaction Expected transaction data
 */
void mock_i2c_expect_transaction(const struct mock_i2c_transaction *transaction);

/**
 * @brief Set I2C device ready state
 * 
 * @param dev Device pointer
 * @param ready Ready state to return
 */
void mock_i2c_set_device_ready(const struct device *dev, bool ready);

/**
 * @brief Verify all expected I2C transactions were called
 */
void mock_i2c_verify_complete(void);

/**
 * @brief Mock I2C write function
 * 
 * @param dev Device pointer
 * @param buf Data to write
 * @param num_bytes Number of bytes to write
 * @param addr Device address
 * @return 0 on success, error code on failure
 */
int i2c_write_mock(const struct device *dev, const uint8_t *buf,
                   uint32_t num_bytes, uint16_t addr);

/**
 * @brief Mock I2C write-read function
 * 
 * @param dev Device pointer
 * @param addr Device address
 * @param write_buf Data to write
 * @param num_write Number of bytes to write
 * @param read_buf Buffer for read data
 * @param num_read Number of bytes to read
 * @return 0 on success, error code on failure
 */
int i2c_write_read_mock(const struct device *dev, uint16_t addr,
                        const void *write_buf, size_t num_write,
                        void *read_buf, size_t num_read);

/* Note: With CONFIG_I2C and CONFIG_GPIO enabled, we use the real Zephyr APIs
 * but provide mock implementations via function interception in the .c files */

#ifdef __cplusplus
}
#endif

#endif /* MOCK_I2C_H */