#
# Copyright (c) 2020 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
cmake_minimum_required(VERSION 3.20.0)

set_property(GLOBAL PROPERTY CSTD c11)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nrf_thingy_env_monitor)

# NORDIC SDK APP START
target_sources(app PRIVATE
	src/main.c
	src/board.c
	src/sensor_manager.c
	src/sensor_hts221_driver.c
	src/sensor_lps22hb_driver.c
	src/sensor_ccs811_driver.c
	src/ble_advertiser.c
	src/battery_service.c
	src/ble_battery_service.c
	src/ess_service.c
	src/ess_encode.c
	src/uptime_service.c
	src/device_naming.c
	)
target_include_directories(app PRIVATE include)

# Add ST register API includes for HTS221 direct register access
target_include_directories(app PRIVATE $ENV{ZEPHYR_BASE}/drivers/sensor/st/stmemsc)
target_include_directories(app PRIVATE ${CMAKE_SOURCE_DIR}/../modules/zephyr/drivers/sensor/st/hts221)
target_include_directories(app PRIVATE ${CMAKE_SOURCE_DIR}/../modules/zephyr/drivers/sensor/st/lps22hb)
target_include_directories(app PRIVATE ${CMAKE_SOURCE_DIR}/../modules/zephyr/drivers/sensor/ams/ccs811)

# NORDIC SDK APP END
