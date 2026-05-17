# Copyright (c) 2026 Eaton
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(unit_ble_advertiser)

# Shared FFF globals.
add_subdirectory(${CMAKE_SOURCE_DIR}/../common/mocks mocks)

set(APP_SRC ${CMAKE_SOURCE_DIR}/../../../src)
set(APP_INC ${CMAKE_SOURCE_DIR}/../../../include)

target_sources(app PRIVATE
	src/test_ble_advertiser.c
	src/mock_ble.c
	src/fake_deps.c
	${APP_SRC}/ble_advertiser.c
)

target_include_directories(app PRIVATE
	src
	${APP_INC}
)

target_link_libraries(app PRIVATE app_test_mocks)

# ble_advertiser.c uses snprintf via <stdio.h> indirectly through Zephyr's
# default libc auto-include chain. Under CONFIG_EXTERNAL_LIBC=y on the
# unit-test target, that chain doesn't fire, so force-include stdio.h
# only for this translation unit. Production builds are unaffected.
set_source_files_properties(
	${APP_SRC}/ble_advertiser.c
	PROPERTIES COMPILE_OPTIONS "-include;stdio.h"
)
