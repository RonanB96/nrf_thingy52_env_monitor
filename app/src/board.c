/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <hal/nrf_gpio.h>

LOG_MODULE_REGISTER(board, CONFIG_LOG_DEFAULT_LEVEL);

/* Device tree node definitions */
#define GPIO0_NODE DT_NODELABEL(gpio0)
#define SX1509B_NODE DT_NODELABEL(sx1509b)

/* GPIO0 hardware constants (nRF52832 specific) */
/* Note: GPIO0 doesn't define ngpios in device tree, but nRF52832 has 32 pins by hardware design */
#define GPIO0_PIN_COUNT 32

/* SX1509B reset pin - matches device tree gpio-hog configuration (P0.16) */
/* This corresponds to: gpios = <&gpio0 16 GPIO_ACTIVE_LOW> in thingy52.overlay */
#define SX1509B_RESET_PIN 16

/* SX1509B initialization values from device tree */
#define SX1509B_INIT_OUT_HIGH DT_PROP(SX1509B_NODE, init_out_high)
#define SX1509B_INIT_OUT_LOW DT_PROP(SX1509B_NODE, init_out_low)
#define SX1509B_NGPIOS DT_PROP(SX1509B_NODE, ngpios)

/* GPIO device references */
static const struct device *gpio0_dev;
static const struct device *sx1509b_dev;

/* GPIO0 pin function definitions from device tree and Thingy:52 schematics */
static const char* get_gpio0_pin_function(int pin)
{
	switch (pin) {
	case 0: return "XL1 (32KHz XTAL)";
	case 1: return "XL2 (32KHz XTAL)";
	case 2: return "UART_RX";
	case 3: return "UART_TX";
	case 4: return "AIN2 (ANALOG)";
	case 5: return "AIN3 (ANALOG)";
	case 6: return "MPU_INT";
	case 7: return "SDA";
	case 8: return "SCL";
	case 9: return "NFC1";
	case 10: return "NFC2";
	case 11: return "Button";
	case 12: return "LIS_INT1";
	case 13: return "USB_DETECT (I2C)";
	case 14: return "SDA_EXT (I2C)";
	case 15: return "SCL_EXT (I2C)";
	case 16: return "SX1509B_RESET (OUTPUT)";
	case 17: return "BAT_CHG_STAT (INPUT)";
	case 18: return "MOS 1";
	case 19: return "MOS 2";
	case 20: return "MOS 3";
	case 21: return "MOS 4";
	case 22: return "CCS811_INT";
	case 23: return "LPS22HB_INT (INPUT)";
	case 24: return "HTS221_INT (INPUT)";
	case 25: return "MIC_DOUT";
	case 26: return "MIC_CLK";
	case 27: return "SPEAKER";
	case 28: return "Battery";
	case 29: return "SPK PWR Ctrl";
	case 30: return "VDD PWR Ctrl";
	case 31: return "BH INT";
	default: return "UNKNOWN";
	}
}



/* GPIO0 pins - intelligent state reading based on pin configuration */
static void print_gpio0_pin_states(void)
{
	LOG_INF("=== GPIO0 (nRF52832) Pin States - INTELLIGENT READ ===");
	LOG_INF("Input pins: Zephyr gpio_pin_get(), Output pins: Direct register access");
	LOG_INF("Pin | Config   | State | Function");
	LOG_INF("----|----------|-------|----------");

	int high_count = 0, low_count = 0, error_count = 0;
	uint32_t output_reg = nrf_gpio_port_out_read(NRF_P0);

	/* Read all GPIO0 pins using appropriate method */
	for (int pin = 0; pin < GPIO0_PIN_COUNT; pin++) {
		/* Get pin configuration */
		nrf_gpio_pin_dir_t dir = nrf_gpio_pin_dir_get(pin);
		nrf_gpio_pin_input_t input = nrf_gpio_pin_input_get(pin);
		nrf_gpio_pin_pull_t pull = nrf_gpio_pin_pull_get(pin);
		const char *function = get_gpio0_pin_function(pin);

		bool pin_state;
		char config_str[12];  /* Increased size for pull info */

		if (dir == NRF_GPIO_PIN_DIR_OUTPUT) {
			/* Output pin - read from output register */
			pin_state = (output_reg & (1UL << pin)) != 0;
			if (input == NRF_GPIO_PIN_INPUT_CONNECT) {
				strncpy(config_str, "OUT+IN", sizeof(config_str) - 1);
			} else {
				strncpy(config_str, "OUTPUT", sizeof(config_str) - 1);
			}
		} else {
			/* Input pin - use Zephyr function and show pull configuration */
			int zephyr_state = gpio_pin_get_raw(gpio0_dev, pin);
			if (zephyr_state >= 0) {
				pin_state = zephyr_state != 0;

				/* Add pull resistor information for input pins */
				switch (pull) {
				case NRF_GPIO_PIN_PULLUP:
					strncpy(config_str, "IN+PULLUP", sizeof(config_str) - 1);
					break;
				case NRF_GPIO_PIN_PULLDOWN:
					strncpy(config_str, "IN+PULLDN", sizeof(config_str) - 1);
					break;
				case NRF_GPIO_PIN_NOPULL:
					strncpy(config_str, "INPUT", sizeof(config_str) - 1);
					break;
				default:
					strncpy(config_str, "IN+UNKN", sizeof(config_str) - 1);
					break;
				}
			} else {
				LOG_WRN("P%02d | INPUT    | ERROR | %s (read failed: %d)", pin, function, zephyr_state);
				error_count++;
				continue;
			}
		}
		config_str[sizeof(config_str) - 1] = '\0';  /* Ensure null termination */

		const char *state_str = pin_state ? "HIGH" : "LOW";
		LOG_INF("P%02d | %-8s | %-5s | %s", pin, config_str, state_str, function);

		if (pin_state) {
			high_count++;
		} else {
			low_count++;
		}
	}

	LOG_INF("GPIO0 Summary: %d HIGH, %d LOW, %d errors, %d total pins",
		high_count, low_count, error_count, GPIO0_PIN_COUNT);
	LOG_INF("Pull types: PULLUP=internal pull-up, PULLDN=internal pull-down, INPUT=no pull");
}/* SX1509B pin function definitions from device tree comments */
static const char* get_sx1509b_pin_function(int pin)
{
	switch (pin) {
	case 0: return "IOEXT_0";
	case 1: return "IOEXT_1";
	case 2: return "IOEXT_2";
	case 3: return "IOEXT_3";
	case 4: return "BAT_MON_EN (OUTPUT)";
	case 5: return "LIGHTWELL_G (OUTPUT_ACTIVE_LOW)";
	case 6: return "LIGHTWELL_B (OUTPUT_ACTIVE_LOW)";
	case 7: return "LIGHTWELL_R (OUTPUT_ACTIVE_LOW)";
	case 8: return "MPU_PWR_CTRL (OUTPUT)";
	case 9: return "MIC_PWR_CTRL (OUTPUT)";
	case 10: return "CCS_PWR_CTRL (OUTPUT)";
	case 11: return "CCS_RESET (OUTPUT)";
	case 12: return "CCS_WAKE (OUTPUT)";
	case 13: return "SENSE_LED_R (OUTPUT)";
	case 14: return "SENSE_LED_G (OUTPUT)";
	case 15: return "SENSE_LED_B (OUTPUT)";
	default: return "UNKNOWN";
	}
}

/* Helper function to build pin list string from bitmask */
static void build_pin_list_string(uint16_t mask, char *buffer, size_t buffer_size)
{
	buffer[0] = '\0';  /* Start with empty string */
	bool first = true;

	for (int pin = 15; pin >= 0; pin--) {  /* Check from high to low for readable order */
		if (mask & (1 << pin)) {
			if (!first) {
				strncat(buffer, ",", buffer_size - strlen(buffer) - 1);
			}
			char pin_str[4];
			snprintf(pin_str, sizeof(pin_str), "%d", pin);
			strncat(buffer, pin_str, buffer_size - strlen(buffer) - 1);
			first = false;
		}
	}

	if (buffer[0] == '\0') {
		strncpy(buffer, "none", buffer_size - 1);
		buffer[buffer_size - 1] = '\0';
	}
}

static void print_sx1509b_pin_states(void)
{
	if (!sx1509b_dev || !device_is_ready(sx1509b_dev)) {
		LOG_ERR("SX1509B device not available - cannot read pin states");
		return;
	}

	/* Build dynamic pin lists from device tree bitmasks */
	char high_pins[64], low_pins[64];
	build_pin_list_string(SX1509B_INIT_OUT_HIGH, high_pins, sizeof(high_pins));
	build_pin_list_string(SX1509B_INIT_OUT_LOW, low_pins, sizeof(low_pins));

	LOG_INF("=== SX1509B GPIO Expander Pin States - ALL PINS ===");
	LOG_INF("Device tree configuration:");
	LOG_INF("  init-out-high: 0x%04X (pins: %s)", SX1509B_INIT_OUT_HIGH, high_pins);
	LOG_INF("  init-out-low:  0x%04X (pins: %s)", SX1509B_INIT_OUT_LOW, low_pins);
	LOG_INF("  ngpios: %d", SX1509B_NGPIOS);
	LOG_INF("");
	LOG_INF("Pin | State | Expected | Function");
	LOG_INF("----|-------|----------|----------");

	int high_count = 0, low_count = 0, error_count = 0, mismatch_count = 0;
	uint16_t init_high = SX1509B_INIT_OUT_HIGH;
	uint16_t init_low = SX1509B_INIT_OUT_LOW;

	for (int pin = 0; pin < SX1509B_NGPIOS; pin++) {
		int pin_state = gpio_pin_get_raw(sx1509b_dev, pin);
		bool expected_high = (init_high & (1 << pin)) != 0;
		bool expected_low = (init_low & (1 << pin)) != 0;
		const char *expected_str = expected_high ? "HIGH" : (expected_low ? "LOW" : "UNDEF");
		const char *function = get_sx1509b_pin_function(pin);

		if (pin_state >= 0) {
			const char *state_str = pin_state ? "HIGH" : "LOW";
			bool state_matches = (pin_state != 0) == expected_high;

			LOG_INF("IO%02d| %-5s | %-8s | %s%s",
				pin,
				state_str,
				expected_str,
				function,
				state_matches ? "" : " *** MISMATCH ***");

			if (pin_state) {
				high_count++;
			} else {
				low_count++;
			}

			if (!state_matches) {
				mismatch_count++;
			}
		} else {
			LOG_WRN("IO%02d| ERROR | %-8s | %s (read failed: %d)",
				pin, expected_str, function, pin_state);
			error_count++;
		}
	}

	LOG_INF("SX1509B Summary: %d HIGH, %d LOW, %d mismatches, %d errors, %d total pins",
		high_count, low_count, mismatch_count, error_count, SX1509B_NGPIOS);
}

static int board_gpio_init(void)
{
	/* Get GPIO0 device */
	gpio0_dev = DEVICE_DT_GET(GPIO0_NODE);
	if (!gpio0_dev || !device_is_ready(gpio0_dev)) {
		LOG_ERR("GPIO0 device not ready!");
		return -ENODEV;
	}

	/* Get SX1509B device */
	sx1509b_dev = DEVICE_DT_GET(SX1509B_NODE);
	if (!sx1509b_dev || !device_is_ready(sx1509b_dev)) {
		LOG_WRN("SX1509B device not ready");
		/* Continue - we can still check GPIO0 pins */
	}

	return 0;
}

void board_print_pin_states(void)
{
	LOG_INF("========================================");
	LOG_INF("=== Thingy:52 GPIO Pin State Report ===");
	LOG_INF("========================================");

	/* Initialize GPIO devices */
	int ret = board_gpio_init();
	if (ret != 0) {
		LOG_ERR("GPIO initialization failed: %d", ret);
		/* Continue with pin state reporting even if init failed */
	}

	/* Print GPIO0 critical pins with intelligent reading */
	print_gpio0_pin_states();
	LOG_INF("");

	/* Print SX1509B expander pins with device tree configuration */
	print_sx1509b_pin_states();
	LOG_INF("");

	LOG_INF("========================================");
	LOG_INF("=== End Pin State Report ===");
	LOG_INF("========================================");
}
