# Copyright (c) 2026 Eaton
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(integration_battery_service)

set(APP_SRC ${CMAKE_SOURCE_DIR}/../../../src)
set(APP_INC ${CMAKE_SOURCE_DIR}/../../../include)

target_sources(app PRIVATE
	src/test_battery_service.c
	${APP_SRC}/battery_service.c
)

target_include_directories(app PRIVATE
	src
	${APP_INC}
)
