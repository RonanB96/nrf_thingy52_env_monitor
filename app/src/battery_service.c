/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/adc/voltage_divider.h>

#include "battery_service.h"

LOG_MODULE_REGISTER(battery_service, CONFIG_LOG_DEFAULT_LEVEL);

/* Battery monitoring configuration - Thingy52 specific */
#define VBATT_NODE DT_PATH(vbatt)

/* Charging detection GPIO configuration */
#define CHARGE_STATUS_NODE DT_PATH(charging, charge_status)

#if DT_NODE_HAS_STATUS(VBATT_NODE, okay)

/* Voltage sensor device */
static const struct device *voltage_dev = DEVICE_DT_GET(VBATT_NODE);

/* Charging detection GPIO configuration */
#if DT_NODE_EXISTS(CHARGE_STATUS_NODE)
static const struct gpio_dt_spec charge_status_gpio = GPIO_DT_SPEC_GET(CHARGE_STATUS_NODE, gpios);
#endif

#define BATTERY_VOLTAGE_FULL_MV 4200   /* Typical LiPo full voltage */
#define BATTERY_VOLTAGE_EMPTY_MV 3000  /* Typical LiPo empty voltage */

/* Battery monitoring data */
struct battery_data {
	uint8_t last_level;
	bool initialized;
	bool last_charging_state;  /* Track last charging state for interrupt logging */
	void (*charging_changed_cb)(bool charging); /* Callback for charging status changes */
};

static struct battery_data bat_data;

/* Thread safety for battery operations */
static K_MUTEX_DEFINE(battery_mutex);

/* GPIO callback for charging status interrupt */
static struct gpio_callback charge_cb_data;

/* Debouncing for charging status interrupt */
#define CHARGE_STATUS_DEBOUNCE_MS 300  /* 300ms debounce time */
static struct k_work_delayable charge_debounce_work;

/* ADC calibration function for battery measurement */
static int battery_adc_calibrate(void)
{
	/* Mirror the voltage driver's internal structure to access the adc_sequence */
	struct voltage_data {
		struct adc_sequence sequence;
		k_timeout_t earliest_sample;
		uint16_t raw;
	};
	
	struct voltage_data *voltage_data = (struct voltage_data *)voltage_dev->data;
	struct adc_sequence *seq = &voltage_data->sequence;
	
	/* Get ADC device from the voltage sensor's configuration */
	struct voltage_divider_dt_spec vdiv_spec = VOLTAGE_DIVIDER_DT_SPEC_GET(VBATT_NODE);
	const struct device *adc_dev = vdiv_spec.port.dev;
	int ret;

	/* Check if ADC device is ready */
	if (!device_is_ready(adc_dev)) {
		LOG_ERR("ADC device not ready for calibration");
		return -ENODEV;
	}

	LOG_INF("Starting ADC calibration for battery channel %d...", 
		vdiv_spec.port.channel_id);
	
	/* Temporarily enable calibration on the voltage device's existing sequence */
	bool original_calibrate = seq->calibrate;
	seq->calibrate = true;
	
	/* Run the calibration using the voltage device's ADC sequence */
	ret = adc_read(adc_dev, seq);
	
	/* Restore original calibrate setting */
	seq->calibrate = original_calibrate;
	
	if (ret != 0) {
		LOG_ERR("ADC calibration failed: %d", ret);
		return ret;
	}

	LOG_INF("ADC calibration completed successfully");
	return 0;
}

/* ADC calibration function like Nordic */
static int battery_sensor_init(void)
{
	int ret;

	if (!device_is_ready(voltage_dev)) {
		LOG_ERR("Voltage sensor device is not ready");
		return -ENODEV;
	}

	/* Perform ADC calibration for accurate battery readings */
	ret = battery_adc_calibrate();
	if (ret != 0) {
		LOG_WRN("ADC calibration failed (%d), continuing with uncalibrated readings", ret);
		/* Don't fail initialization, just warn */
	}

	LOG_INF("Battery voltage sensor initialized");
	return 0;
}

/* Simple lithium battery voltage to percentage conversion
 * Based on typical LiPo discharge curve
 * Thingy52 voltage divider: full_ohms=1.68MΩ, output_ohms=180kΩ
 * Battery range: ~4.2V (100%) to ~3.0V (0%)
 * ADC measures: ~450mV (100%) to ~320mV (0%)
 */
static uint8_t voltage_to_percentage(uint32_t voltage_mv)
{
	/* Nordic's method: voltage_mv is already the actual battery voltage after divider correction */
	LOG_DBG("Battery voltage: %d mV", voltage_mv);

	if (voltage_mv >= BATTERY_VOLTAGE_FULL_MV) {
		return 100;
	} else if (voltage_mv <= BATTERY_VOLTAGE_EMPTY_MV) {
		return 0;
	}

	/* Linear interpolation between empty and full voltage */
	return (uint8_t)((voltage_mv - BATTERY_VOLTAGE_EMPTY_MV) * 100 /
			(BATTERY_VOLTAGE_FULL_MV - BATTERY_VOLTAGE_EMPTY_MV));
}

/* Debounced charging status work handler */
static void charge_status_debounce_work(struct k_work *work)
{
#if DT_NODE_EXISTS(CHARGE_STATUS_NODE)
	bool is_charging = gpio_pin_get_dt(&charge_status_gpio) == 0; /* Active low */

	/* Only update if state changed */
	if (is_charging != bat_data.last_charging_state) {
		bat_data.last_charging_state = is_charging;
		LOG_INF("Battery charging status changed: %s", is_charging ? "CHARGING" : "NOT CHARGING");

		/* Notify callback if registered */
		if (bat_data.charging_changed_cb) {
			bat_data.charging_changed_cb(is_charging);
		}
	}
#endif
}

/* Charging status interrupt handler */
static void charge_status_interrupt(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
#if DT_NODE_EXISTS(CHARGE_STATUS_NODE)
	/* Cancel any pending debounce work and schedule a new one */
	k_work_cancel_delayable(&charge_debounce_work);
	k_work_schedule(&charge_debounce_work, K_MSEC(CHARGE_STATUS_DEBOUNCE_MS));
#endif
}



static int battery_sample(void)
{
	int ret;
	struct sensor_value voltage;

	if (!bat_data.initialized) {
		LOG_ERR("Battery service not initialized");
		return -EINVAL;
	}



	/* Fetch sensor data - this handles power management and ADC reading */
	ret = sensor_sample_fetch(voltage_dev);
	if (ret != 0) {
		LOG_ERR("Failed to fetch voltage sensor data: %d", ret);
		goto cleanup;
	}

	/* Get voltage reading */
	ret = sensor_channel_get(voltage_dev, SENSOR_CHAN_VOLTAGE, &voltage);
	if (ret != 0) {
		LOG_ERR("Failed to get voltage channel: %d", ret);
		goto cleanup;
	}

	/* Convert to millivolts */
	uint32_t battery_voltage_mv = sensor_value_to_double(&voltage) * 1000;

	uint8_t percentage = voltage_to_percentage(battery_voltage_mv);

	/* Check charging status for debug output */
	bool is_charging = false;
#if DT_NODE_EXISTS(CHARGE_STATUS_NODE)
	is_charging = gpio_pin_get_dt(&charge_status_gpio) == 0; /* Active low */
#endif

	printk("Battery: %d mV -> %d%% %s\n", battery_voltage_mv, percentage,
	       is_charging ? "(Charging)" : "");

	bat_data.last_level = percentage;

cleanup:
	return ret;
}

int battery_service_init(void)
{
	int ret;

	LOG_INF("Initializing battery service");

	/* Initialize voltage sensor */
	ret = battery_sensor_init();
	if (ret != 0) {
		LOG_ERR("Failed to initialize voltage sensor: %d", ret);
		return ret;
	}

	/* Configure charging detection GPIO */
#if DT_NODE_EXISTS(CHARGE_STATUS_NODE)
	if (!gpio_is_ready_dt(&charge_status_gpio)) {
		LOG_ERR("Charge status GPIO device not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&charge_status_gpio, GPIO_INPUT);
	if (ret != 0) {
		LOG_ERR("Failed to configure charge status GPIO: %d", ret);
		return ret;
	}

	/* Configure interrupt for charging status changes */
	ret = gpio_pin_interrupt_configure_dt(&charge_status_gpio, GPIO_INT_EDGE_BOTH);
	if (ret != 0) {
		LOG_ERR("Failed to configure charge status interrupt: %d", ret);
		return ret;
	}

	/* Initialize GPIO callback */
	gpio_init_callback(&charge_cb_data, charge_status_interrupt, BIT(charge_status_gpio.pin));
	ret = gpio_add_callback(charge_status_gpio.port, &charge_cb_data);
	if (ret != 0) {
		LOG_ERR("Failed to add charge status callback: %d", ret);
		return ret;
	}

	/* Initialize charging state */
	bat_data.last_charging_state = gpio_pin_get_dt(&charge_status_gpio) == 0;
	LOG_INF("Initial charging state: %s", bat_data.last_charging_state ? "CHARGING" : "NOT CHARGING");

	/* Initialize debounce work */
	k_work_init_delayable(&charge_debounce_work, charge_status_debounce_work);
#endif

	/* Mark as initialized */
	bat_data.initialized = true;

	/* Take an initial measurement to avoid reporting 0% initially */
	ret = battery_sample();
	if (ret != 0) {
		LOG_WRN("Initial battery measurement failed (%d), will retry later", ret);
		/* Set a reasonable default instead of 0 */
		bat_data.last_level = 50;
	}

	LOG_INF("Battery service initialized successfully");
	return 0;
}

uint8_t battery_service_get_level(void)
{
	int err = battery_sample();
	if (err != 0) {
		LOG_ERR("Battery service failed to read a sample! %d", err);
		return 50; /* Return a reasonable default */
	}

	return bat_data.last_level;
}

int battery_service_sample(void)
{
	return battery_sample();
}

bool battery_service_is_charging(void)
{
#if DT_NODE_EXISTS(CHARGE_STATUS_NODE)
	if (!bat_data.initialized) {
		LOG_WRN("Battery service not initialized");
		return false;
	}

	/* Always read fresh charging status from GPIO - don't cache it */
	/* Charge status pin is active low - low means charging */
	bool current_charging = gpio_pin_get_dt(&charge_status_gpio) == 0;

	/* Update cached state for interrupt detection */
	if (current_charging != bat_data.last_charging_state) {
		bat_data.last_charging_state = current_charging;

		/* Notify callback if registered */
		if (bat_data.charging_changed_cb) {
			bat_data.charging_changed_cb(current_charging);
		}
	}

	return current_charging;
#else
	return false;
#endif
}

void battery_service_register_charging_callback(void (*callback)(bool charging))
{
	bat_data.charging_changed_cb = callback;
}

#else /* !DT_NODE_HAS_STATUS(VBATT_NODE, okay) */

int battery_service_init(void)
{
	LOG_WRN("Battery monitoring not available - no vbatt node in devicetree");
	return -ENOTSUP;
}

uint8_t battery_service_get_level(void)
{
	return 0;
}

#endif /* DT_NODE_HAS_STATUS(VBATT_NODE, okay) */
