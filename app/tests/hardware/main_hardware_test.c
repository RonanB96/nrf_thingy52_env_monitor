/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file main_hardware_test.c
 * @brief Main Hardware-in-the-Loop test runner for Thingy:52
 * 
 * This is the main entry point for running comprehensive hardware validation tests.
 * It coordinates execution of all HIL test modules and provides integrated reporting.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include "hardware_test.h"
#include "board.h"

LOG_MODULE_REGISTER(main_hardware_test, CONFIG_LOG_DEFAULT_LEVEL);

/**
 * @brief Run complete hardware test suite
 */
void run_complete_hardware_test_suite(void)
{
    LOG_INF("========================================");
    LOG_INF("=== Thingy:52 Hardware-in-the-Loop Test Suite ===");
    LOG_INF("========================================");
    LOG_INF("Starting comprehensive hardware validation...");
    
    /* Allow system initialization to complete */
    k_msleep(1000);
    
    /* Phase 1: GPIO and Hardware Infrastructure Tests */
    LOG_INF("\n>>> PHASE 1: GPIO and Hardware Infrastructure <<<");
    run_all_gpio_hardware_tests();
    
    k_msleep(2000); /* Allow stabilization between test phases */
    
    /* Phase 2: Sensor Hardware Tests */
    LOG_INF("\n>>> PHASE 2: Environmental Sensor Hardware <<<");
    run_all_hardware_sensor_tests();
    
    k_msleep(2000);
    
    /* Phase 3: Power Management Tests */
    LOG_INF("\n>>> PHASE 3: Power Management and Optimization <<<");
    run_all_power_hardware_tests();
    
    k_msleep(2000);
    
    /* Phase 4: BLE Connectivity Tests */
    LOG_INF("\n>>> PHASE 4: BLE Connectivity and Services <<<");
    run_all_ble_hardware_tests();
    
    /* Final Summary */
    LOG_INF("\n========================================");
    LOG_INF("=== Hardware Test Suite Complete ===");
    LOG_INF("========================================");
    LOG_INF("All hardware validation phases completed.");
    LOG_INF("Check individual test reports above for detailed results.");
    LOG_INF("Use external tools (nRF Connect, Power Profiler) for manual validation.");
    LOG_INF("========================================");
}

/**
 * @brief Main function for hardware test firmware
 */
int main(void)
{
    LOG_INF("Thingy:52 Hardware-in-the-Loop Test Firmware");
    LOG_INF("Nordic nRF Connect SDK v3.0.2");
    LOG_INF("========================================");
    
    /* Settings subsystem initialization */
    int ret = settings_subsys_init();
    if (ret) {
        LOG_WRN("Settings subsystem init failed: %d", ret);
    }
    
    /* Allow GPIO hog driver and device tree initialization to complete */
    LOG_INF("Waiting for hardware initialization...");
    k_msleep(2000);
    
    /* Print initial pin states for debugging */
    LOG_INF("Initial hardware state:");
    board_print_pin_states();
    
    /* Run the complete hardware test suite */
    run_complete_hardware_test_suite();
    
    /* Keep running for manual testing and monitoring */
    LOG_INF("Hardware test firmware ready for manual testing.");
    LOG_INF("Connect with nRF Connect mobile app for BLE validation.");
    LOG_INF("Use Nordic Power Profiler Kit for power measurements.");
    
    while (1) {
        /* Periodic heartbeat for monitoring */
        LOG_INF("Hardware test firmware running... (use for manual validation)");
        k_msleep(30000); /* 30 second intervals */
    }
    
    return 0;
}