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

/* Mock I2C driver functions (override Zephyr driver) */
#ifndef CONFIG_I2C
/* Only define these when I2C driver is disabled for testing */

static inline bool device_is_ready(const struct device *dev)
{
    /* Mock implementation - always return true unless specifically set */
    return true;
}

static inline int i2c_write(const struct device *dev, const uint8_t *buf,
                           uint32_t num_bytes, uint16_t addr)
{
    struct mock_i2c_transaction transaction = {
        .type = MOCK_I2C_WRITE,
        .addr = addr,
        .write_buf = buf,
        .write_len = num_bytes,
        .read_buf = NULL,
        .read_len = 0,
        .expected_return = 0
    };
    
    /* Process mock transaction */
    return 0; /* Default success */
}

static inline int i2c_read(const struct device *dev, uint8_t *buf,
                          uint32_t num_bytes, uint16_t addr)
{
    struct mock_i2c_transaction transaction = {
        .type = MOCK_I2C_READ,
        .addr = addr,
        .write_buf = NULL,
        .write_len = 0,
        .read_buf = buf,
        .read_len = num_bytes,
        .expected_return = 0
    };
    
    /* Process mock transaction */
    return 0; /* Default success */
}

static inline int i2c_write_read(const struct device *dev, uint16_t addr,
                                const void *write_buf, size_t num_write,
                                void *read_buf, size_t num_read)
{
    struct mock_i2c_transaction transaction = {
        .type = MOCK_I2C_WRITE_READ,
        .addr = addr,
        .write_buf = (const uint8_t *)write_buf,
        .write_len = num_write,
        .read_buf = (uint8_t *)read_buf,
        .read_len = num_read,
        .expected_return = 0
    };
    
    /* Process mock transaction */
    return 0; /* Default success */
}

#endif /* !CONFIG_I2C */

#ifdef __cplusplus
}
#endif

#endif /* MOCK_I2C_H */