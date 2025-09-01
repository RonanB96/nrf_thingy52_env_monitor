/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */


#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/sys/printk.h>
#include "ble_advertiser.h"
#include "device_naming.h"

LOG_MODULE_REGISTER(ble_advertiser, CONFIG_LOG_DEFAULT_LEVEL);

/* Advertisement data array indices */
enum ad_data_index {
  AD_FLAGS = 0,
  AD_NAME = 1,
  AD_SERVICE_UUIDS = 2,
  AD_APPEARANCE = 3,
  AD_DATA_COUNT /* Total number of advertisement elements */
};

/* Using Zephyr's standard service UUID definitions:
 * BT_UUID_ESS_VAL (Environmental Sensing Service - 0x181A)
 * BT_UUID_BAS_VAL (Battery Service - 0x180F)
 * BT_UUID_DIS_VAL (Device Information Service - 0x180A)
 * All defined in zephyr/bluetooth/uuid.h
 */

/* Advertising state */
static bool advertising_enabled = false;

/* Bluetooth ready synchronization */
static K_SEM_DEFINE(bt_ready_sem, 0, 1);

/* Legacy advertising parameters - ultra low power for months of battery life */
static const struct bt_le_adv_param adv_param = {
    .id = BT_ID_DEFAULT,
    .options = BT_LE_ADV_OPT_CONN,
    .interval_min = BT_GAP_ADV_SLOW_INT_MIN,  /* 1.0 seconds (1600 * 0.625ms) */
    .interval_max = BT_GAP_SCAN_SLOW_INTERVAL_2,  /* 2.56 s */
};

/* Advertisement data structure - minimal data for device discovery */
static struct bt_data ad_data[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS,
                  BT_LE_AD_NO_BREDR),         /* No BR/EDR support */
    BT_DATA(BT_DATA_NAME_COMPLETE, NULL, 0), /* Complete name - set dynamically */
    BT_DATA_BYTES(BT_DATA_UUID16_ALL,
                  BT_UUID_16_ENCODE(BT_UUID_ESS_VAL),    /* Environmental Sensing Service */
                  BT_UUID_16_ENCODE(BT_UUID_BAS_VAL),    /* Battery Service */
                  BT_UUID_16_ENCODE(BT_UUID_DIS_VAL)),   /* Device Information Service */
    /* Appearance: Multi-Sensor (0x0552) */
    BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE, BT_UUID_16_ENCODE(BT_APPEARANCE_SENSOR_MULTI)),
};

static void ble_disconnected(struct bt_conn *conn, uint8_t reason)
{
  LOG_INF("Device disconnected (reason %u)", reason);

  /* Note: Do not restart advertising here - connection object not yet freed.
   * Advertising will be restarted in the recycled callback when connection
   * object is actually freed and reusable.
   */
}

static void ble_recycled(void)
{
  LOG_INF("Connection recycled - restarting advertising");

  /* Now it's safe to restart advertising as connection object is freed */
  int ret = bt_le_adv_start(&adv_param, ad_data, ARRAY_SIZE(ad_data), NULL, 0);
  if (ret) {
    LOG_ERR("Failed to restart advertising after recycle: %d (%s)", ret,
            ret == -ENOMEM ? "ENOMEM (out of memory)" : "other error");
  } else {
    advertising_enabled = true;
    LOG_INF("Advertising restarted successfully after recycle");
  }
}

static void ble_connected(struct bt_conn *conn, uint8_t err)
{
  LOG_INF("Device connected (err %d)", err);

  /* Stop advertising when connected */
  if (advertising_enabled) {
    int ret = bt_le_adv_stop();
    if (ret) {
      LOG_WRN("Failed to stop advertising on connect: %d", ret);
    } else {
      advertising_enabled = false;
      LOG_INF("Advertising stopped on connect");
    }
  }
}

static struct bt_conn_cb conn_callbacks = {
  .connected = ble_connected,
  .disconnected = ble_disconnected,
  .recycled = ble_recycled,
};

/* Generate unique device name from hardware ID */
static void generate_unique_device_name(char *name_buffer, size_t buffer_size) {
  /* Use the device naming service for consistent naming */
  int ret = device_naming_get_name(name_buffer, buffer_size);

  if (ret != 0) {
    /* Fallback if device naming service unavailable */
    uint8_t device_id[8];
    ssize_t id_len = hwinfo_get_device_id(device_id, sizeof(device_id));

    if (id_len > 0) {
      uint32_t unique_id = 0;
      size_t bytes_to_use = MIN(4, id_len);

      for (size_t i = 0; i < bytes_to_use; i++) {
        unique_id = (unique_id << 8) | device_id[id_len - 1 - i];
      }

      snprintf(name_buffer, buffer_size, "%s-%08X", DEVICE_PREFIX, unique_id);
    } else {
      strncpy(name_buffer, DEVICE_PREFIX "-00000000", buffer_size - 1);
      name_buffer[buffer_size - 1] = '\0';
    }
  }

  LOG_INF("Generated unique device name: %s", name_buffer);
}

static int update_advertisement_data(const struct ble_sensor_data *data) {
  /* Generate unique device name */
  static char device_name[32];
  static bool name_generated = false;

  if (!name_generated) {
    generate_unique_device_name(device_name, sizeof(device_name));
    name_generated = true;
  }

  ad_data[AD_NAME].data = device_name;
  ad_data[AD_NAME].data_len = strlen(device_name);
  (void)data;

  return 0;
}

static void bt_ready_cb(int err) {
  LOG_INF("Bluetooth ready callback called with err=%d", err);

  if (err) {
    LOG_ERR("Bluetooth init failed: %d", err);
    return;
  }

  LOG_INF("Bluetooth initialized successfully, signaling semaphore");

  /* Signal that Bluetooth is ready */
  k_sem_give(&bt_ready_sem);

  LOG_INF("Semaphore signaled successfully");
}

int ble_advertiser_init(void) {
  int ret;

  printk("Initializing BLE advertiser\n");

  /* Enable Bluetooth with callback - all BLE operations happen in callback */
  ret = bt_enable(bt_ready_cb);
  if (ret) {
    printk("Bluetooth init failed: %d\n", ret);
    return ret;
  }

  bt_conn_cb_register(&conn_callbacks);

  printk("BLE advertiser initialization started - waiting for Bluetooth ready\n");
  return 0;
}
int ble_advertiser_start(const struct ble_sensor_data *data) {
  int ret;

  if (!data) {
    LOG_ERR("Invalid data parameter");
    return -EINVAL;
  }

  /* Wait for Bluetooth to be ready (timeout after 10 seconds) */
  LOG_INF("Waiting for Bluetooth to be ready...");
  ret = k_sem_take(&bt_ready_sem, K_SECONDS(10));
  if (ret != 0) {
    LOG_ERR("Timeout waiting for Bluetooth to be ready: %d", ret);
    return ret;
  }

  LOG_INF("Bluetooth is ready, proceeding with advertising setup");

  update_advertisement_data(data);

  /* Give the Bluetooth controller extra time to fully initialize */
  k_msleep(500);

  /* Stop any existing advertising first */
  if (advertising_enabled) {
    LOG_INF("Stopping existing advertising before restart");
    ret = bt_le_adv_stop();
    if (ret) {
      LOG_WRN("Failed to stop existing advertising: %d", ret);
    }
    advertising_enabled = false;
    k_msleep(100); /* Give time for stop to complete */
  }

  /* Start legacy advertising with retry logic */
  LOG_INF("Starting BLE legacy advertising...");
  for (int retry = 0; retry < 5; retry++) {
    ret = bt_le_adv_start(&adv_param, ad_data, ARRAY_SIZE(ad_data), NULL, 0);
    if (ret == 0) {
      advertising_enabled = true;
      LOG_INF("Legacy advertising started successfully");
      break; /* Success */
    }

    LOG_WRN("Legacy advertising start failed: %d (%s), attempt %d/5", ret,
            ret == -EAGAIN   ? "EAGAIN"
            : ret == -EINVAL ? "EINVAL"
                             : "OTHER",
            retry + 1);
    if (ret == -EAGAIN) {
      /* Bluetooth busy, wait longer and retry */
      k_msleep(200 + (100 * retry)); /* Longer backoff */
      continue;
    } else {
      /* Other error, don't retry */
      LOG_ERR("Failed to start legacy advertising: %d", ret);
      return ret;
    }
  }

  if (ret) {
    LOG_ERR("Failed to start legacy advertising after retries: %d", ret);
    return ret;
  }

  return 0;
}

int ble_advertiser_stop(void) {
  int ret = 0;

  /* Stop legacy advertising */
  if (advertising_enabled) {
    ret = bt_le_adv_stop();
    if (ret) {
      LOG_ERR("Failed to stop legacy advertising: %d", ret);
    } else {
      LOG_INF("Stopped BLE legacy advertising");
    }
    advertising_enabled = false;
  }

  return ret;
}