# Copyright (c) 2024 Nordic Semiconductor ASA
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(unit_uptime_service)

# Shared FFF mocks (bt_gatt_*, DEFINE_FFF_GLOBALS).
add_subdirectory(${CMAKE_SOURCE_DIR}/../common/mocks mocks)

target_sources(app PRIVATE
	src/test_uptime_service.c
	# Production unit under test:
	${CMAKE_SOURCE_DIR}/../../../src/uptime_service.c
)

target_include_directories(app PRIVATE
	${CMAKE_SOURCE_DIR}/../../../include
)

target_link_libraries(app PRIVATE app_test_mocks)
