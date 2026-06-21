# Copyright (c) 2024 Nordic Semiconductor ASA
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(unit_ess_service)

# Shared FFF mocks: bt_gatt_*, sensor_manager_*, DEFINE_FFF_GLOBALS.
add_subdirectory(${CMAKE_SOURCE_DIR}/../common/mocks mocks)

target_sources(app PRIVATE
	src/test_ess_service.c
	# Production unit under test, plus its real ess_encode dependency
	# (already 100% unit-tested under tests/unit/ess_encode/).
	${CMAKE_SOURCE_DIR}/../../../src/ess_service.c
	${CMAKE_SOURCE_DIR}/../../../src/ess_encode.c
)

target_include_directories(app PRIVATE
	${CMAKE_SOURCE_DIR}/../../../include
)

target_link_libraries(app PRIVATE app_test_mocks)
