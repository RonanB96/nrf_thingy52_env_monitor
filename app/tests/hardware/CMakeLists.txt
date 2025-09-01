#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

set_property(GLOBAL PROPERTY CSTD c11)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(thingy52_hardware_test)

# HARDWARE TEST BUILD
target_sources(app PRIVATE
    main_hardware_test.c
    test_hardware_sensors.c
    test_hardware_gpio.c
    test_hardware_ble.c
    test_hardware_power.c
)

# Include hardware test headers
target_include_directories(app PRIVATE .)

# Include main application sources for hardware access
target_sources(app PRIVATE
    ../../src/board.c
    ../../src/sensor_manager.c
    ../../src/sensor_hts221_driver.c
    ../../src/sensor_lps22hb_driver.c
    ../../src/sensor_ccs811_driver.c
    ../../src/ble_advertiser.c
    ../../src/battery_service.c
    ../../src/ble_battery_service.c
    ../../src/ess_service.c
    ../../src/uptime_service.c
    ../../src/device_naming.c
)

# Include main application headers
target_include_directories(app PRIVATE ../../include)

# Add ST register API includes for HTS221 direct register access
target_include_directories(app PRIVATE $ENV{ZEPHYR_BASE}/drivers/sensor/st/stmemsc)