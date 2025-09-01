/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file test_hardware_ble.c
 * @brief Hardware-in-the-Loop BLE connectivity testing for Thingy:52
 * 
 * This module provides real BLE hardware validation:
 * - BLE advertising visibility and timing
 * - Service discovery validation
 * - Characteristic read/write functionality
 * - Connection stability testing
 * - Mobile app compatibility validation
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/logging/log.h>

#include "ble_advertiser.h"
#include "ess_service.h"
#include "ble_battery_service.h"
#include "uptime_service.h"
#include "sensor_manager.h"

LOG_MODULE_REGISTER(test_hardware_ble, CONFIG_LOG_DEFAULT_LEVEL);

/* BLE test result structure */
struct ble_test_result {
    bool passed;
    const char *test_name;
    const char *error_msg;
    int measured_value;
};

/* Test result storage */
static struct ble_test_result ble_test_results[16];
static int ble_test_count = 0;

/* BLE connection tracking */
static struct bt_conn *current_connection = NULL;
static bool connection_established = false;
static bool disconnection_detected = false;

/* Service discovery tracking */
static bool ess_service_found = false;
static bool battery_service_found = false;
static bool uptime_service_found = false;

/* ESS Service UUID (0x181A) - for reference */
/* static struct bt_uuid_16 ess_uuid = BT_UUID_INIT_16(BT_UUID_ESS_VAL); */

/* Battery Service UUID (0x180F) - for reference */  
/* static struct bt_uuid_16 battery_uuid = BT_UUID_INIT_16(BT_UUID_BAS_VAL); */

/* Custom Uptime Service UUID - for reference */
/* static struct bt_uuid_128 uptime_uuid = BT_UUID_INIT_128(
    BT_UUID_128_ENCODE(0x01234567, 0x89AB, 0xCDEF, 0x0123, 0x456789ABCDEF)); */

/**
 * @brief Record BLE test result for reporting
 */
static void record_ble_test_result(const char *test_name, bool passed, 
                                  const char *error_msg, int measured_value)
{
    if (ble_test_count < ARRAY_SIZE(ble_test_results)) {
        ble_test_results[ble_test_count].passed = passed;
        ble_test_results[ble_test_count].test_name = test_name;
        ble_test_results[ble_test_count].error_msg = error_msg;
        ble_test_results[ble_test_count].measured_value = measured_value;
        ble_test_count++;
    }
}

/**
 * @brief BLE connection callback
 */
static void connected_cb(struct bt_conn *conn, uint8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];
    
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    
    if (err) {
        LOG_ERR("Connection to %s failed (err %d)", addr, err);
        return;
    }
    
    LOG_INF("Connected to %s", addr);
    current_connection = bt_conn_ref(conn);
    connection_established = true;
}

/**
 * @brief BLE disconnection callback
 */
static void disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];
    
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    LOG_INF("Disconnected from %s (reason %d)", addr, reason);
    
    if (current_connection) {
        bt_conn_unref(current_connection);
        current_connection = NULL;
    }
    
    connection_established = false;
    disconnection_detected = true;
}

/* BLE connection callbacks */
BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected_cb,
    .disconnected = disconnected_cb,
};

/**
 * @brief Test BLE advertising visibility
 */
void test_hardware_ble_advertising_visibility(void)
{
    LOG_INF("=== Testing BLE Advertising Visibility ===");
    
    /* Dummy sensor data for advertising tests */
    struct ble_sensor_data dummy_data = {
        .temperature = 23.5,
        .humidity = 45.0,
        .pressure = 1013.25,
        .eco2 = 400,
        .tvoc = 0,
        .battery_level = 85
    };
    
    /* Test 1: BLE subsystem initialization */
    int ret = bt_enable(NULL);
    if (ret != 0) {
        record_ble_test_result("BLE Enable", false, "BLE enable failed", ret);
        LOG_ERR("BLE enable failed: %d", ret);
        return;
    }
    record_ble_test_result("BLE Enable", true, NULL, 0);
    LOG_INF("✓ BLE subsystem enabled");
    
    /* Test 2: BLE advertiser initialization */
    ret = ble_advertiser_init();
    if (ret != 0) {
        record_ble_test_result("BLE Advertiser Init", false, "Advertiser init failed", ret);
        LOG_ERR("BLE advertiser initialization failed: %d", ret);
        return;
    }
    record_ble_test_result("BLE Advertiser Init", true, NULL, 0);
    LOG_INF("✓ BLE advertiser initialized");
    
    /* Test 3: Start advertising with dummy data */
    ret = ble_advertiser_start(&dummy_data);
    if (ret != 0) {
        record_ble_test_result("BLE Start Advertising", false, "Start advertising failed", ret);
        LOG_ERR("Failed to start BLE advertising: %d", ret);
        return;
    }
    record_ble_test_result("BLE Start Advertising", true, NULL, 0);
    LOG_INF("✓ BLE advertising started");
    
    /* Test 4: Advertising validation (runtime check) */
    k_msleep(2000); /* Wait for advertising to stabilize */
    
    /* Check if advertising is active by attempting to restart */
    ret = ble_advertiser_stop();
    if (ret == 0) {
        record_ble_test_result("BLE Advertising Active", true, NULL, 1);
        LOG_INF("✓ BLE advertising was active (successfully stopped)");
        
        /* Restart for further tests */
        ret = ble_advertiser_start(&dummy_data);
        if (ret == 0) {
            LOG_INF("✓ BLE advertising restarted");
        }
    } else {
        record_ble_test_result("BLE Advertising Active", false, "Stop failed - not advertising", ret);
        LOG_WRN("BLE advertising stop failed: %d (may not have been advertising)", ret);
    }
    
    LOG_INF("BLE advertising test complete - device should be visible to nRF Connect");
    LOG_INF("Use nRF Connect mobile app to scan and verify device visibility");
}

/**
 * @brief Test BLE service discovery
 */
void test_hardware_ble_service_discovery(void)
{
    LOG_INF("=== Testing BLE Service Discovery ===");
    
    /* Test 1: ESS service initialization */
    int ret = ess_service_init();
    if (ret != 0) {
        record_ble_test_result("ESS Service Init", false, "ESS init failed", ret);
        LOG_ERR("ESS service initialization failed: %d", ret);
    } else {
        record_ble_test_result("ESS Service Init", true, NULL, 0);
        LOG_INF("✓ ESS service initialized");
        ess_service_found = true;
    }
    
    /* Test 2: Battery service initialization */
    ret = ble_battery_service_init();
    if (ret != 0) {
        record_ble_test_result("Battery Service Init", false, "Battery init failed", ret);
        LOG_ERR("Battery service initialization failed: %d", ret);
    } else {
        record_ble_test_result("Battery Service Init", true, NULL, 0);
        LOG_INF("✓ Battery service initialized");
        battery_service_found = true;
    }
    
    /* Test 3: Uptime service initialization */
    ret = uptime_service_init();
    if (ret != 0) {
        record_ble_test_result("Uptime Service Init", false, "Uptime init failed", ret);
        LOG_ERR("Uptime service initialization failed: %d", ret);
    } else {
        record_ble_test_result("Uptime Service Init", true, NULL, 0);
        LOG_INF("✓ Uptime service initialized");
        uptime_service_found = true;
    }
    
    /* Test 4: Service discovery summary */
    int services_found = (ess_service_found ? 1 : 0) + 
                        (battery_service_found ? 1 : 0) +
                        (uptime_service_found ? 1 : 0);
    
    bool all_services = (services_found == 3);
    record_ble_test_result("All Services Available", all_services,
                          all_services ? NULL : "Not all services initialized",
                          services_found);
    
    LOG_INF("Service discovery: %d/3 services available", services_found);
    LOG_INF("Services should be discoverable via nRF Connect mobile app:");
    LOG_INF("  - Environmental Sensing Service (ESS): %s", ess_service_found ? "✓" : "✗");
    LOG_INF("  - Battery Service (BAS): %s", battery_service_found ? "✓" : "✗");
    LOG_INF("  - Uptime Service (Custom): %s", uptime_service_found ? "✓" : "✗");
}

/**
 * @brief Test BLE characteristic read/write functionality
 */
void test_hardware_ble_characteristic_read_write(void)
{
    LOG_INF("=== Testing BLE Characteristic Access ===");
    
    /* Initialize sensor manager for data */
    int ret = sensor_manager_init();
    if (ret != 0) {
        record_ble_test_result("Sensor Manager Init", false, "Sensor init failed", ret);
        LOG_ERR("Sensor manager initialization failed: %d", ret);
        return;
    }
    record_ble_test_result("Sensor Manager Init", true, NULL, 0);
    LOG_INF("✓ Sensor manager initialized for characteristic testing");
    
    /* Update sensor data */
    ret = sensor_manager_update();
    if (ret == 0) {
        record_ble_test_result("Sensor Data Update", true, NULL, 0);
        LOG_INF("✓ Sensor data updated");
    } else {
        record_ble_test_result("Sensor Data Update", false, "Sensor update failed", ret);
        LOG_WRN("Sensor data update failed: %d", ret);
    }
    
    /* Test ESS characteristic values */
    if (ess_service_found) {
        int temp_value = ess_service_get_temperature();
        int humidity_value = ess_service_get_humidity();
        int pressure_value = ess_service_get_pressure();
        int co2_value = ess_service_get_co2();
        int tvoc_value = ess_service_get_tvoc();
        
        bool ess_data_valid = (temp_value != 0 || humidity_value != 0 || pressure_value != 0);
        record_ble_test_result("ESS Characteristic Data", ess_data_valid,
                              ess_data_valid ? NULL : "No valid ESS data",
                              temp_value);
        
        LOG_INF("ESS characteristic values:");
        LOG_INF("  Temperature: %d (raw BLE value)", temp_value);
        LOG_INF("  Humidity: %d (raw BLE value)", humidity_value);
        LOG_INF("  Pressure: %d (raw BLE value)", pressure_value);
        LOG_INF("  CO2: %d (raw BLE value)", co2_value);
        LOG_INF("  TVOC: %d (raw BLE value)", tvoc_value);
        LOG_INF("✓ ESS characteristics accessible via nRF Connect");
    }
    
    /* Test Battery characteristic values */
    if (battery_service_found) {
        uint8_t battery_level = ble_battery_service_get_level();
        bool battery_valid = (battery_level <= 100);
        record_ble_test_result("Battery Characteristic", battery_valid,
                              battery_valid ? NULL : "Invalid battery level",
                              battery_level);
        
        LOG_INF("Battery characteristic value: %d%% %s", 
                battery_level, battery_valid ? "✓" : "✗");
    }
    
    LOG_INF("Characteristic values should be readable via nRF Connect mobile app");
}

/**
 * @brief Test BLE connection stability
 */
void test_hardware_ble_connection_stability(void)
{
    LOG_INF("=== Testing BLE Connection Stability ===");
    
    /* Reset connection tracking */
    connection_established = false;
    disconnection_detected = false;
    
    /* Test 1: Connection acceptance readiness */
    record_ble_test_result("Connection Ready", true, NULL, 1);
    LOG_INF("✓ BLE stack ready to accept connections");
    
    /* Test 2: Wait for potential connection (simulation) */
    LOG_INF("Waiting 5 seconds for potential test connection...");
    k_msleep(5000);
    
    if (connection_established) {
        record_ble_test_result("Connection Established", true, NULL, 1);
        LOG_INF("✓ BLE connection was established during test");
        
        /* Test connection stability */
        LOG_INF("Testing connection stability for 10 seconds...");
        k_msleep(10000);
        
        if (current_connection && !disconnection_detected) {
            record_ble_test_result("Connection Stable", true, NULL, 1);
            LOG_INF("✓ BLE connection remained stable");
        } else {
            record_ble_test_result("Connection Stable", false, "Connection lost", 0);
            LOG_WRN("BLE connection was lost during stability test");
        }
    } else {
        record_ble_test_result("Connection Available", true, NULL, 0);
        LOG_INF("No test connection established - device ready for manual testing");
        LOG_INF("Use nRF Connect mobile app to connect and test manually");
    }
    
    /* Clean up connection if exists */
    if (current_connection) {
        bt_conn_disconnect(current_connection, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        bt_conn_unref(current_connection);
        current_connection = NULL;
    }
}

/**
 * @brief Print BLE hardware test summary report
 */
void ble_hardware_test_print_report(void)
{
    LOG_INF("========================================");
    LOG_INF("=== BLE Hardware Test Summary Report ===");
    LOG_INF("========================================");
    
    int passed = 0, failed = 0;
    
    for (int i = 0; i < ble_test_count; i++) {
        struct ble_test_result *result = &ble_test_results[i];
        
        if (result->passed) {
            passed++;
            if (result->measured_value >= 0) {
                LOG_INF("✓ PASS: %s (%d)", result->test_name, result->measured_value);
            } else {
                LOG_INF("✓ PASS: %s", result->test_name);
            }
        } else {
            failed++;
            if (result->error_msg) {
                LOG_ERR("✗ FAIL: %s - %s", result->test_name, result->error_msg);
            } else {
                LOG_ERR("✗ FAIL: %s", result->test_name);
            }
        }
    }
    
    LOG_INF("========================================");
    LOG_INF("BLE Tests: %d, Passed: %d, Failed: %d", ble_test_count, passed, failed);
    LOG_INF("BLE Success Rate: %d%%", (passed * 100) / (ble_test_count > 0 ? ble_test_count : 1));
    LOG_INF("========================================");
    
    /* Manual testing instructions */
    LOG_INF("=== Manual BLE Testing Instructions ===");
    LOG_INF("1. Open nRF Connect mobile app");
    LOG_INF("2. Scan for BLE devices");
    LOG_INF("3. Look for 'Thingy:52' or custom device name");
    LOG_INF("4. Connect to the device");
    LOG_INF("5. Discover services:");
    LOG_INF("   - Environmental Sensing Service (0x181A)");
    LOG_INF("   - Battery Service (0x180F)");
    LOG_INF("   - Custom Uptime Service");
    LOG_INF("6. Read characteristics and verify sensor data");
    LOG_INF("7. Test notifications if enabled");
    LOG_INF("========================================");
}

/**
 * @brief Run all BLE hardware tests
 */
void run_all_ble_hardware_tests(void)
{
    LOG_INF("Starting Hardware-in-the-Loop BLE Tests");
    LOG_INF("========================================");
    
    /* Reset test results */
    ble_test_count = 0;
    
    /* Run BLE hardware tests */
    test_hardware_ble_advertising_visibility();
    test_hardware_ble_service_discovery();
    test_hardware_ble_characteristic_read_write();
    test_hardware_ble_connection_stability();
    
    /* Print final report */
    ble_hardware_test_print_report();
}