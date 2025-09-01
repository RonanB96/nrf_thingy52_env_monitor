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

# Simple test source files - no complex mocking for now
target_sources(app PRIVATE
    src/test_main.c
)

# Include directories for tests
target_include_directories(app PRIVATE 
    src
    ../include
)

# Compile definitions for testing
target_compile_definitions(app PRIVATE
    -DCONFIG_ZTEST=1
    -DZTEST_UNITTEST=1
)