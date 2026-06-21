#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(unit_sensor_manager_policy)

target_sources(app PRIVATE
	src/test_sensor_data_contract.c
)

target_include_directories(app PRIVATE
	${CMAKE_SOURCE_DIR}/../../../include
)

# Pull in the application Kconfig so CONFIG_SENSOR_ENV_INTERVAL_SEC etc. are
# defined for the test build (matches the production build).
set_property(GLOBAL PROPERTY APP_KCONFIG_FILE ${CMAKE_SOURCE_DIR}/../../../Kconfig)
