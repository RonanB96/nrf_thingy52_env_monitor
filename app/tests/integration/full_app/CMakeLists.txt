# Copyright (c) 2026 Eaton
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(integration_full_app)

set(APP_SRC ${CMAKE_SOURCE_DIR}/../../../src)
set(APP_INC ${CMAKE_SOURCE_DIR}/../../../include)
set(ZMOD ${CMAKE_SOURCE_DIR}/../../../../modules/zephyr)

target_sources(app PRIVATE
	# Test driver and minimal i2c emul backends.
	src/test_full_app.c
	emul/hts221_emul.c
	emul/lps22hb_emul.c
	emul/ccs811_emul.c
	# Production wrappers under coverage.
	${APP_SRC}/sensor_hts221_driver.c
	${APP_SRC}/sensor_lps22hb_driver.c
	${APP_SRC}/sensor_ccs811_driver.c
)

target_include_directories(app PRIVATE
	src
	${APP_INC}
	# Private upstream driver headers are included by the wrappers.
	${ZMOD}/drivers/sensor/st/hts221
	${ZMOD}/drivers/sensor/st/lps22hb
	${ZMOD}/drivers/sensor/ams/ccs811
	$ENV{ZEPHYR_BASE}/drivers/sensor/st/stmemsc
)

# Twister enables -Werror=unused-const-variable= which trips on a couple of
# reserved-for-future-use constants in the production sensor wrappers. We are
# not allowed to modify app/src/*.c here, so suppress it for the test build
# only. Production builds (west build app) are unaffected.
set_source_files_properties(
	${APP_SRC}/sensor_ccs811_driver.c
	${APP_SRC}/sensor_hts221_driver.c
	${APP_SRC}/sensor_lps22hb_driver.c
	PROPERTIES COMPILE_OPTIONS "-Wno-error=unused-const-variable;-Wno-error=unused-variable;-Wno-error=unused-function"
)
