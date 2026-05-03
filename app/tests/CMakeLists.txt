#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nrf_thingy_env_monitor_tests)

# Test source files with mocking framework
target_sources(app PRIVATE
    src/test_main.c
    src/test_battery_service.c
    src/test_ess_service.c
    src/test_mock_framework.c
    src/test_sensor_manager.c
    src/test_sensor_manager_policy.c
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