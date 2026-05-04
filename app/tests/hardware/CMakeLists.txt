#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(thingy52_hil_sensors)

target_sources(app PRIVATE
    src/test_hil_hts221.c
    src/test_hil_lps22hb.c
    src/test_hil_ccs811.c
    src/test_hil_battery.c
    src/test_hil_power_rails.c
)

target_include_directories(app PRIVATE
    src
    ${ZEPHYR_BASE}/drivers/sensor/st/lps22hb
)
