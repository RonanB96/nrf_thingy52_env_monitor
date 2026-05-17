/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/util.h>
#include <zephyr/settings/settings.h>
#include <zephyr/kernel.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/types.h>
#include <bluetooth/mesh/models.h>
#include <bluetooth/mesh/sensor_types.h>
#include <bluetooth/mesh/sensor.h>
#include <bluetooth/mesh/sensor_srv.h>
#include <float.h>
#include "model_handler.h"
#include "battery_service.h"
#include "sensor_manager.h"

/* Thingy52 has HTS221 sensor for temperature and humidity */
#if DT_NODE_HAS_STATUS(DT_NODELABEL(hts221), okay)
/** Thingy52 */
#define SENSOR_NODE        DT_NODELABEL(hts221)
#define TEMP_DATA_TYPE     SENSOR_CHAN_AMBIENT_TEMP
#define HUMIDITY_DATA_TYPE SENSOR_CHAN_HUMIDITY
#elif DT_NODE_HAS_STATUS(DT_NODELABEL(temp), okay)
/** nRF52 DK */
#define SENSOR_NODE        DT_NODELABEL(temp)
#define TEMP_DATA_TYPE     SENSOR_CHAN_DIE_TEMP
/* No humidity sensor on DK */
#define HUMIDITY_DATA_TYPE SENSOR_CHAN_HUMIDITY
#else
#error "Unsupported board!"
#endif

/* Require LPS22HB pressure sensor */
#if !DT_NODE_HAS_STATUS(DT_NODELABEL(lps22hb_press), okay)
#error "LPS22HB pressure sensor not found or not enabled in device tree!"
#endif
#define PRESSURE_SENSOR_NODE DT_NODELABEL(lps22hb_press)
#define PRESSURE_DATA_TYPE   SENSOR_CHAN_PRESS

/* Require CCS811 gas sensor */
#if !DT_NODE_HAS_STATUS(DT_NODELABEL(ccs811), okay)
#error "CCS811 gas sensor not found or not enabled in device tree!"
#endif
#define GAS_SENSOR_NODE DT_NODELABEL(ccs811)
#define CO2_DATA_TYPE   SENSOR_CHAN_CO2
#define VOC_DATA_TYPE   SENSOR_CHAN_VOC

#define TEMP_INIT(_val)                                                                            \
	{                                                                                          \
		.format = &bt_mesh_sensor_format_temp,                                             \
		.raw = { FIELD_GET(GENMASK(7, 0), (_val) * 100),                                   \
			 FIELD_GET(GENMASK(15, 8), (_val) * 100) }                                 \
	}

#define HUMIDITY_INIT(_val)                                                                        \
	{                                                                                          \
		.format = &bt_mesh_sensor_format_percentage_16,                                    \
		.raw = { FIELD_GET(GENMASK(7, 0), (_val) * 100),                                   \
			 FIELD_GET(GENMASK(15, 8), (_val) * 100) }                                 \
	}

#define CO2_INIT(_val)                                                                             \
	{                                                                                          \
		.format = &bt_mesh_sensor_format_co2_concentration,                                \
		.raw = { FIELD_GET(GENMASK(7, 0), (_val)),                                         \
			 FIELD_GET(GENMASK(15, 8), (_val)) }                                       \
	}

static const struct device *env_sensor_dev = DEVICE_DT_GET(SENSOR_NODE);
static const struct device *pressure_sensor_dev = DEVICE_DT_GET(PRESSURE_SENSOR_NODE);
static const struct device *gas_sensor_dev = DEVICE_DT_GET(GAS_SENSOR_NODE);

static uint32_t temp_sample_count;
static uint32_t humidity_sample_count;
static uint32_t pressure_sample_count;
static uint32_t eco2_sample_count;
static uint32_t tvoc_sample_count;

/*******************************************************************************
 * Temperature Sensor
 ******************************************************************************/
static int temperature_get(struct bt_mesh_sensor_srv *srv, struct bt_mesh_sensor *sensor,
			   struct bt_mesh_msg_ctx *ctx, struct bt_mesh_sensor_value *rsp)
{
	float temperature;
	int err;

	/* Get temperature from sensor manager */
	temperature = sensor_manager_get_temperature();
	if (temperature == 0.0f) {
		printk("Error: No valid temperature data available\n");
		return -ENODATA;
	}

	/* Convert float to sensor_value for BLE Mesh */
	struct sensor_value sensor_val = {
		.val1 = (int32_t)temperature,
		.val2 = (int32_t)((temperature - (int32_t)temperature) * 1000000)};

	err = bt_mesh_sensor_value_from_sensor_value(sensor->type->channels[0].format, &sensor_val,
						     rsp);
	if (err && err != -ERANGE) {
		printk("Error encoding temperature sensor data (%d)\n", err);
		return err;
	}

	temp_sample_count++;
	printk("Temperature: %s (sample #%d)\n", bt_mesh_sensor_ch_str(rsp), temp_sample_count);
	return 0;
}

static const struct bt_mesh_sensor_descriptor temp_descriptor = {
	.tolerance =
		{
			.negative = BT_MESH_SENSOR_TOLERANCE_ENCODE(4),
			.positive = BT_MESH_SENSOR_TOLERANCE_ENCODE(4),
		},
	.sampling_type = BT_MESH_SENSOR_SAMPLING_INSTANTANEOUS,
};

static struct bt_mesh_sensor temperature_sensor = {
	.type = &bt_mesh_sensor_present_dev_op_temp,
	.get = temperature_get,
	.descriptor = &temp_descriptor,
};

/*******************************************************************************
 * Humidity Sensor
 ******************************************************************************/
static int humidity_get(struct bt_mesh_sensor_srv *srv, struct bt_mesh_sensor *sensor,
			struct bt_mesh_msg_ctx *ctx, struct bt_mesh_sensor_value *rsp)
{
	float humidity;
	int err;

	/* Get humidity from sensor manager */
	humidity = sensor_manager_get_humidity();
	if (humidity == 0.0f) {
		printk("Error: No valid humidity data available\n");
		return -ENODATA;
	}

	/* Convert float to sensor_value for BLE Mesh */
	struct sensor_value sensor_val = {
		.val1 = (int32_t)humidity,
		.val2 = (int32_t)((humidity - (int32_t)humidity) * 1000000)};

	err = bt_mesh_sensor_value_from_sensor_value(sensor->type->channels[0].format, &sensor_val,
						     rsp);
	if (err && err != -ERANGE) {
		printk("Error encoding humidity sensor data (%d)\n", err);
		return err;
	}

	humidity_sample_count++;
	printk("Humidity: %s (sample #%d)\n", bt_mesh_sensor_ch_str(rsp), humidity_sample_count);
	return 0;
}

static const struct bt_mesh_sensor_descriptor humidity_descriptor = {
	.tolerance =
		{
			.negative = BT_MESH_SENSOR_TOLERANCE_ENCODE(3),
			.positive = BT_MESH_SENSOR_TOLERANCE_ENCODE(3),
		},
	.sampling_type = BT_MESH_SENSOR_SAMPLING_INSTANTANEOUS,
};

static struct bt_mesh_sensor humidity_sensor = {
	.type = &bt_mesh_sensor_present_amb_rel_humidity,
	.get = humidity_get,
	.descriptor = &humidity_descriptor,
};

/*******************************************************************************
 * Pressure Sensor (LPS22HB)
 ******************************************************************************/
static int pressure_get(struct bt_mesh_sensor_srv *srv, struct bt_mesh_sensor *sensor,
			struct bt_mesh_msg_ctx *ctx, struct bt_mesh_sensor_value *rsp)
{
	float pressure;
	int err;
	pressure_sample_count++;

	/* Get pressure from sensor manager */
	pressure = sensor_manager_get_pressure();
	if (pressure == 0.0f) {
		printk("Error: No valid pressure data available\n");
		return -ENODATA;
	}

	/* Convert from hPa to Pa for Bluetooth Mesh */
	uint32_t pressure_pa = (uint32_t)(pressure * 100); /* hPa to Pa */

	err = bt_mesh_sensor_value_from_micro(sensor->type->channels[0].format,
					      pressure_pa * 1000000LL, rsp);
	if (err && err != -ERANGE) {
		printk("Error encoding pressure sensor data (%d)\n", err);
		return err;
	}

	printk("Pressure: %s Pa (sample #%d)\n", bt_mesh_sensor_ch_str(rsp), pressure_sample_count);
	return 0;
}

static const struct bt_mesh_sensor_descriptor pressure_descriptor = {
	.tolerance =
		{
			.negative = BT_MESH_SENSOR_TOLERANCE_ENCODE(2),
			.positive = BT_MESH_SENSOR_TOLERANCE_ENCODE(2),
		},
	.sampling_type = BT_MESH_SENSOR_SAMPLING_INSTANTANEOUS,
};

static struct bt_mesh_sensor pressure_sensor = {
	.type = &bt_mesh_sensor_air_pressure,
	.get = pressure_get,
	.descriptor = &pressure_descriptor,
};

/*******************************************************************************
 * CO2 Sensor (CCS811)
 ******************************************************************************/
static int co2_get(struct bt_mesh_sensor_srv *srv, struct bt_mesh_sensor *sensor,
		   struct bt_mesh_msg_ctx *ctx, struct bt_mesh_sensor_value *rsp)
{
	float eco2;
	int err;
	eco2_sample_count++;

	/* Get eCO2 from sensor manager */
	eco2 = sensor_manager_get_eco2();
	if (eco2 == 0.0f) {
		printk("Error: No valid eCO2 data available\n");
		return -ENODATA;
	}

	/* eCO2 is already in ppm, convert to micro ppm for BT Mesh */
	uint32_t eco2_ppm = (uint32_t)eco2;

	err = bt_mesh_sensor_value_from_micro(sensor->type->channels[0].format,
					      eco2_ppm * 1000000LL, rsp);
	if (err && err != -ERANGE) {
		printk("Error encoding eCO2 sensor data (%d)\n", err);
		return err;
	}

	printk("eCO2: %s ppm (sample #%d)\n", bt_mesh_sensor_ch_str(rsp), eco2_sample_count);
	return 0;
}

static const struct bt_mesh_sensor_descriptor co2_descriptor = {
	.tolerance =
		{
			.negative = BT_MESH_SENSOR_TOLERANCE_ENCODE(5),
			.positive = BT_MESH_SENSOR_TOLERANCE_ENCODE(5),
		},
	.sampling_type = BT_MESH_SENSOR_SAMPLING_INSTANTANEOUS,
};

static struct bt_mesh_sensor co2_sensor = {
	.type = &bt_mesh_sensor_present_amb_co2_concentration,
	.get = co2_get,
	.descriptor = &co2_descriptor,
};

/*******************************************************************************
 * VOC Sensor (CCS811)
 ******************************************************************************/
static int voc_get(struct bt_mesh_sensor_srv *srv, struct bt_mesh_sensor *sensor,
		   struct bt_mesh_msg_ctx *ctx, struct bt_mesh_sensor_value *rsp)
{
	float tvoc;
	int err;
	tvoc_sample_count++;

	/* Get TVOC from sensor manager */
	tvoc = sensor_manager_get_tvoc();
	if (tvoc == 0.0f) {
		printk("Error: No valid TVOC data available\n");
		return -ENODATA;
	}

	/* TVOC is already in ppb, convert to micro ppb for BT Mesh */
	uint32_t tvoc_ppb = (uint32_t)tvoc;

	err = bt_mesh_sensor_value_from_micro(sensor->type->channels[0].format,
					      tvoc_ppb * 1000000LL, rsp);
	if (err && err != -ERANGE) {
		printk("Error encoding TVOC sensor data (%d)\n", err);
		return err;
	}

	printk("TVOC: %s ppb (sample #%d)\n", bt_mesh_sensor_ch_str(rsp), tvoc_sample_count);
	return 0;
}

static const struct bt_mesh_sensor_descriptor voc_descriptor = {
	.tolerance =
		{
			.negative = BT_MESH_SENSOR_TOLERANCE_ENCODE(5),
			.positive = BT_MESH_SENSOR_TOLERANCE_ENCODE(5),
		},
	.sampling_type = BT_MESH_SENSOR_SAMPLING_INSTANTANEOUS,
};

static struct bt_mesh_sensor voc_sensor = {
	.type = &bt_mesh_sensor_present_amb_voc_concentration,
	.get = voc_get,
	.descriptor = &voc_descriptor,
};

/*******************************************************************************
 * Battery Level Sensor
 ******************************************************************************/
static int battery_get(struct bt_mesh_sensor_srv *srv, struct bt_mesh_sensor *sensor,
		       struct bt_mesh_msg_ctx *ctx, struct bt_mesh_sensor_value *rsp)
{
	int err;

	/* Get battery percentage from sensor manager */
	uint8_t battery_percentage = sensor_manager_get_battery_level();
	if (battery_percentage == 0) {
		printk("Error: No valid battery data available\n");
		return -ENODATA;
	}

	/* Convert to sensor value using proper encoding - percentage as micro units */
	err = bt_mesh_sensor_value_from_micro(sensor->type->channels[0].format,
					      battery_percentage * 1000000LL, rsp);
	if (err && err != -ERANGE) {
		printk("Error encoding battery sensor data (%d)\n", err);
		return err;
	}

	printk("Battery sensor read: %d%%\n", battery_percentage);
	return 0;
}

static const struct bt_mesh_sensor_descriptor battery_descriptor = {
	.tolerance =
		{
			.negative = BT_MESH_SENSOR_TOLERANCE_ENCODE(2),
			.positive = BT_MESH_SENSOR_TOLERANCE_ENCODE(2),
		},
	.sampling_type = BT_MESH_SENSOR_SAMPLING_INSTANTANEOUS,
};

static struct bt_mesh_sensor battery_sensor = {
	.type = &bt_mesh_sensor_present_dev_op_efficiency, /* Using device efficiency as battery
							      level representation */
	.get = battery_get,
	.descriptor = &battery_descriptor,
};

/*******************************************************************************
 * Sensor Array and Server Setup with Publication
 ******************************************************************************/
static struct bt_mesh_sensor *const env_sensors[] = {
	&temperature_sensor, &humidity_sensor, &pressure_sensor,
	&co2_sensor,         &voc_sensor,      &battery_sensor,
};

static struct bt_mesh_sensor_srv env_sensor_srv =
	BT_MESH_SENSOR_SRV_INIT(env_sensors, ARRAY_SIZE(env_sensors));

/*******************************************************************************
 * Periodic Sensor Publishing - Optimized for ultra-low power
 ******************************************************************************/
/* Environmental sensors change slowly - optimize for weeks of battery life */
#define SENSOR_PUBLISH_INTERVAL_MS  (30 * 60 * 1000)     // 30 minutes
#define CCS811_WARMUP_TIME_MS       (5 * 60 * 1000)      // 5 minutes initial warmup
#define BATTERY_PUBLISH_INTERVAL_MS (6 * 60 * 60 * 1000) // 6 hours for battery

/* Dedicated work queue for sensor operations to avoid system work queue overflow */
#define SENSOR_WORKQUEUE_STACK_SIZE 2048
#define SENSOR_WORKQUEUE_PRIORITY   5

static K_THREAD_STACK_DEFINE(sensor_workqueue_stack, SENSOR_WORKQUEUE_STACK_SIZE);
struct k_work_q sensor_work_q;
static struct k_work_delayable sensor_publish_work;
static struct k_work_delayable battery_publish_work;

static bool ccs811_warmed_up = false;
static int64_t startup_time;
static int sensor_publish_count = 0; // Track publish cycles for battery timing

static void sensor_publish_handler(struct k_work *work)
{
	int err;

	if (!bt_mesh_is_provisioned()) {
		printk("Device not provisioned, skipping sensor publish\n");
		goto reschedule;
	}

	/* Check if CCS811 has warmed up */
	int64_t current_time = k_uptime_get();
	if (!ccs811_warmed_up && (current_time - startup_time) >= CCS811_WARMUP_TIME_MS) {
		ccs811_warmed_up = true;
		printk("CCS811 warm-up complete - readings now accurate\n");
	}

	printk("Publishing environmental sensors (cycle #%d)...\n", ++sensor_publish_count);

	/* Publish environmental sensors (skip battery - it has its own schedule) */
	for (int i = 0; i < ARRAY_SIZE(env_sensors); i++) {
		struct bt_mesh_sensor_value val; /* Local allocation per iteration */

		/* Skip battery sensor - it has its own schedule */
		if (env_sensors[i] == &battery_sensor) {
			continue;
		}

		/* Skip CO2 and VOC sensors during warm-up period */
		if ((env_sensors[i] == &co2_sensor || env_sensors[i] == &voc_sensor) &&
		    !ccs811_warmed_up) {
			printk("Skipping %s sensor - still warming up (%lld/%d ms)\n",
			       env_sensors[i] == &co2_sensor ? "CO2" : "VOC",
			       current_time - startup_time, CCS811_WARMUP_TIME_MS);
			continue;
		}

		err = env_sensors[i]->get(&env_sensor_srv, env_sensors[i], NULL, &val);

		if (!err) {
			err = bt_mesh_sensor_srv_pub(&env_sensor_srv, NULL, env_sensors[i], &val);
			if (err) {
				printk("Error publishing sensor %d (%d)\n", i, err);
			} else {
				printk("Published sensor %d successfully\n", i);
			}
		} else {
			printk("Error reading sensor %d (%d)\n", i, err);
		}

		/* Small delay between sensor publications to avoid flooding */
		k_sleep(K_MSEC(100));
	}

reschedule:
	/* Reschedule for next publication on dedicated work queue */
	k_work_reschedule_for_queue(&sensor_work_q, &sensor_publish_work,
				    K_MSEC(SENSOR_PUBLISH_INTERVAL_MS));
}

static void battery_publish_handler(struct k_work *work)
{
	struct bt_mesh_sensor_value val;
	int err;

	if (!bt_mesh_is_provisioned()) {
		printk("Device not provisioned, skipping battery publish\n");
		goto reschedule;
	}

	printk("Publishing battery sensor...\n");

	/* Sample battery and publish */
	err = battery_service_sample();
	if (err) {
		printk("Failed to sample battery (%d)\n", err);
	}

	err = battery_sensor.get(&env_sensor_srv, &battery_sensor, NULL, &val);
	if (!err) {
		err = bt_mesh_sensor_srv_pub(&env_sensor_srv, NULL, &battery_sensor, &val);
		if (err) {
			printk("Error publishing battery sensor (%d)\n", err);
		} else {
			printk("Published battery sensor successfully\n");
		}
	} else {
		printk("Error reading battery sensor (%d)\n", err);
	}

reschedule:
	/* Reschedule battery for next publication (much less frequent) */
	k_work_reschedule_for_queue(&sensor_work_q, &battery_publish_work,
				    K_MSEC(BATTERY_PUBLISH_INTERVAL_MS));
}

/*******************************************************************************
 * Health Server Callbacks
 ******************************************************************************/

static const struct bt_mesh_health_srv_cb health_srv_cb = {
	.attn_on = NULL,
	.attn_off = NULL,
};

static struct bt_mesh_health_srv health_srv = {
	.cb = &health_srv_cb,
};

BT_MESH_HEALTH_PUB_DEFINE(health_pub, 0);

/*******************************************************************************
 * Bluetooth Mesh Elements
 ******************************************************************************/
static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(1,
		     BT_MESH_MODEL_LIST(BT_MESH_MODEL_CFG_SRV,
					BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
					BT_MESH_MODEL_SENSOR_SRV(&env_sensor_srv)),
		     BT_MESH_MODEL_NONE),
};

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

/*******************************************************************************
 * Initialization
 ******************************************************************************/
const struct bt_mesh_comp *model_handler_init(void)
{
	printk("Initializing environmental sensors...\n");

	/* Check HTS221 sensor (temperature and humidity) - REQUIRED */
	if (!device_is_ready(env_sensor_dev)) {
		printk("✗ HTS221 sensor (%s) not ready! FAILING.\n", env_sensor_dev->name);
		return NULL;
	} else {
		printk("✓ HTS221 sensor (%s) ready\n", env_sensor_dev->name);
	}

	/* Check LPS22HB pressure sensor - REQUIRED */
	if (!device_is_ready(pressure_sensor_dev)) {
		printk("✗ LPS22HB pressure sensor not ready! FAILING.\n");
		return NULL;
	} else {
		printk("✓ LPS22HB pressure sensor ready\n");
	}

	/* Check CCS811 gas sensor - REQUIRED */
	if (!device_is_ready(gas_sensor_dev)) {
		printk("✗ CCS811 gas sensor not ready! FAILING.\n");
		return NULL;
	} else {
		printk("✓ CCS811 gas sensor ready\n");
	}

	/* Initialize dedicated work queue for sensor operations */
	k_work_queue_init(&sensor_work_q);
	k_work_queue_start(&sensor_work_q, sensor_workqueue_stack,
			   K_THREAD_STACK_SIZEOF(sensor_workqueue_stack), SENSOR_WORKQUEUE_PRIORITY,
			   NULL);

	/* Initialize periodic sensor publishing with different schedules */
	k_work_init_delayable(&sensor_publish_work, sensor_publish_handler);
	k_work_init_delayable(&battery_publish_work, battery_publish_handler);

	/* Record startup time for CCS811 warm-up tracking */
	startup_time = k_uptime_get();

	if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
		if (settings_subsys_init()) {
			printk("Failed to initialize settings subsystem\n");
		}
	}

	printk("Environment Monitor initialized successfully with all sensors:\n");
	printk("  - Temperature: HTS221\n");
	printk("  - Humidity: HTS221\n");
	printk("  - Pressure: LPS22HB\n");
	printk("  - CO2: CCS811\n");
	printk("  - VOC: CCS811\n");
	printk("  - Battery: ADC\n");
	printk("Starting ultra-low power sensor publishing:\n");
	printk("  - Environmental sensors every %d minutes\n",
	       SENSOR_PUBLISH_INTERVAL_MS / (60 * 1000));
	printk("  - Battery sensor every %d hours\n",
	       BATTERY_PUBLISH_INTERVAL_MS / (60 * 60 * 1000));

	/* Start periodic sensor publishing with staggered delays using dedicated work queue */
	k_work_reschedule_for_queue(&sensor_work_q, &sensor_publish_work, K_SECONDS(5));
	k_work_reschedule_for_queue(&sensor_work_q, &battery_publish_work,
				    K_SECONDS(30)); /* Battery starts 30s later */

	return &comp;
}
