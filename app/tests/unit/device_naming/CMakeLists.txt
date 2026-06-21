#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(unit_device_naming)

target_sources(app PRIVATE
        src/test_device_naming.c
        src/mock_hwinfo.c
        # Production unit under test:
        ${CMAKE_SOURCE_DIR}/../../../src/device_naming.c
)

target_include_directories(app PRIVATE
        src
        ${CMAKE_SOURCE_DIR}/../../../include
)
