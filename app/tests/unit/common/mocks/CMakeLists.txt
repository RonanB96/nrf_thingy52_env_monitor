# Copyright (c) 2024 Nordic Semiconductor ASA
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
# Shared FFF mock library for unit tests. Patterned on
# zephyrproject-rtos/zephyr tests/bluetooth/audio/mocks/CMakeLists.txt:
# tests link the bits they need rather than each suite redefining fakes.

add_library(app_test_mocks STATIC
	bt_gatt.c
	fff_globals.c
	sensor_manager_mock.c
)

target_include_directories(app_test_mocks PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../../../../include
)

target_link_libraries(app_test_mocks PUBLIC zephyr_interface)
