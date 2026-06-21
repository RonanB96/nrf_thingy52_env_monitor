#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(unit_ess_encode)

target_sources(app PRIVATE
        src/test_ess_encode.c
        # Production unit under test:
        ${CMAKE_SOURCE_DIR}/../../../src/ess_encode.c
)

target_include_directories(app PRIVATE
        ${CMAKE_SOURCE_DIR}/../../../include
)
