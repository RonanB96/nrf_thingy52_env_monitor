# Copyright (c) 2024 Nordic Semiconductor ASA
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(unit_sensor_manager)

# Shared FFF mocks (sensor_manager_mock.* is excluded; the real production
# module is the unit under test, not a fake — link only the bits we need).
add_subdirectory(${CMAKE_SOURCE_DIR}/../common/mocks mocks)

target_sources(app PRIVATE
	src/test_sensor_manager.c
	src/mock_drivers.c
	src/stub_devices.c
	# Reuse the battery_service FFF mock from the battery_service suite —
	# zero duplication.
	${CMAKE_SOURCE_DIR}/../battery_service/src/mock_battery_service.c
	# Production unit under test:
	${CMAKE_SOURCE_DIR}/../../../src/sensor_manager.c
)

target_include_directories(app PRIVATE
	src
	${CMAKE_SOURCE_DIR}/../battery_service/src
	${CMAKE_SOURCE_DIR}/../../../include
)

# Link against shared mocks for DEFINE_FFF_GLOBALS only; suppress the
# sensor_manager_mock since this binary defines the real sensor_manager
# functions itself (they live in the production object file we just added).
target_link_libraries(app PRIVATE app_test_mocks)
