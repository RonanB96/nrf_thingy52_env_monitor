/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file hardware_test.h
 * @brief Hardware-in-the-Loop test function declarations for Thingy:52
 */

#ifndef HARDWARE_TEST_H
#define HARDWARE_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

/* Sensor hardware test functions */
void test_hardware_hts221_temperature_humidity_reading(void);
void test_hardware_lps22hb_pressure_reading(void);
void test_hardware_ccs811_air_quality_reading(void);
void test_hardware_sensor_power_cycling(void);
void hardware_test_print_report(void);
void run_all_hardware_sensor_tests(void);

/* GPIO hardware test functions */
void test_hardware_sx1509b_initialization_sequence(void);
void test_hardware_ccs811_power_control_via_sx1509b(void);
void test_hardware_gpio_pin_state_validation(void);
void test_hardware_device_tree_initialization_order(void);
void gpio_hardware_test_print_report(void);
void run_all_gpio_hardware_tests(void);

/* BLE hardware test functions */
void test_hardware_ble_advertising_visibility(void);
void test_hardware_ble_service_discovery(void);
void test_hardware_ble_characteristic_read_write(void);
void test_hardware_ble_connection_stability(void);
void ble_hardware_test_print_report(void);
void run_all_ble_hardware_tests(void);

/* Power hardware test functions */
void test_hardware_power_consumption_baseline(void);
void test_hardware_sensor_power_optimization(void);
void test_hardware_sleep_mode_power_usage(void);
void test_hardware_battery_monitoring_accuracy(void);
void test_hardware_power_measurement_infrastructure(void);
void power_hardware_test_print_report(void);
void run_all_power_hardware_tests(void);

/* Main hardware test runner */
void run_complete_hardware_test_suite(void);

#ifdef __cplusplus
}
#endif

#endif /* HARDWARE_TEST_H */