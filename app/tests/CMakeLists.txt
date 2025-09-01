#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
cmake_minimum_required(VERSION 3.20.0)

# Set C standard to match main application
set_property(GLOBAL PROPERTY CSTD c11)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nrf_thingy_env_monitor_tests)

# Test source files
target_sources(app PRIVATE
    src/test_main.c
    src/test_sensor_manager.c
    src/test_ess_service.c
    src/test_battery_service.c
)

# Mock implementations
target_sources(app PRIVATE
    src/mocks/mock_i2c.c
    src/mocks/mock_gpio.c
    src/mocks/mock_sensor.c
    src/mocks/mock_bluetooth.c
)

# Include directories for tests
target_include_directories(app PRIVATE 
    src
    src/mocks
    ../include
)

# Include the main application source files that we want to test
# Note: We need to be selective to avoid conflicts with test mocks
# Temporarily comment out to test basic framework first
# target_sources(app PRIVATE
#     ../src/sensor_manager.c
#     ../src/ess_service.c
#     ../src/battery_service.c
#     ../src/device_naming.c
# )

# Include ST register API for HTS221 (same as main app)
target_include_directories(app PRIVATE $ENV{ZEPHYR_BASE}/drivers/sensor/st/stmemsc)

# Compile definitions for testing
target_compile_definitions(app PRIVATE
    -DCONFIG_ZTEST=1
    -DZTEST_UNITTEST=1
)