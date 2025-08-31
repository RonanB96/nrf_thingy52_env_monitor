/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef BOARD_H_
#define BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Print all GPIO pin states for debugging
 *
 * This function prints a comprehensive report of all GPIO pin states
 * on both the nRF52832 (GPIO0) and SX1509B GPIO expander.
 * Useful for debugging hardware configurations and pin states.
 *
 * The report includes:
 * - All 32 GPIO0 pins with their functions and states
 * - All 16 SX1509B pins with their functions and states
 * - Critical pin analysis (SX1509B reset, CCS811 power, etc.)
 * - Summary statistics
 */
void board_print_pin_states(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H_ */
