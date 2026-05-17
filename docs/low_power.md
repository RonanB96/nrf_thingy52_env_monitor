# Low-power budget

This document records the configuration choices that drive average current on
the Thingy:52 (nRF52832 + sensors) and the rationale for each. It deliberately
does **not** quote current numbers that have not been derived from a primary
source available in this repository or measured on this hardware. The
"Per-state current" section is intentionally empty until either:

1. a copy of each component datasheet and the Thingy:52 schematic is added
   under `docs/datasheets/` (or equivalent) and the relevant table / page is
   cited next to every number, or
2. real values are captured with a Power Profiler Kit II in series with the
   Thingy:52 battery and the raw capture is attached.

Anything else would be guesswork.

## Configured operating points (verifiable from the source tree)

| Item                          | Value                                                                | Source                                                                                              |
| ----------------------------- | -------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------- |
| Advertising type              | Legacy connectable, 1 Mbps                                           | [`app/src/ble_advertiser.c`](../app/src/ble_advertiser.c) (`adv_param`)                             |
| Advertising interval          | `BT_GAP_ADV_SLOW_INT_MIN` … `BT_GAP_SCAN_SLOW_INTERVAL_2`            | [`app/src/ble_advertiser.c`](../app/src/ble_advertiser.c)                                           |
| TX power Kconfig              | `CONFIG_BT_CTLR_TX_PWR_0=y`                                          | [`app/prj.conf`](../app/prj.conf)                                                                   |
| LE Privacy (RPA)              | enabled                                                              | [`app/prj.conf`](../app/prj.conf) (`CONFIG_BT_CTLR_PRIVACY=y`)                                      |
| Env sample period (connected) | `CONFIG_SENSOR_ENV_INTERVAL_SEC` (default 900 s)                     | [`app/Kconfig`](../app/Kconfig)                                                                     |
| Env sampling (disconnected)   | suspended                                                            | [`app/src/sensor_manager.c`](../app/src/sensor_manager.c) — `env_work` gated by `connected_count`   |
| CCS811 sample period          | `CONFIG_SENSOR_ENV_INTERVAL_SEC × CONFIG_SENSOR_AIR_QUALITY_DIVISOR` | [`app/Kconfig`](../app/Kconfig)                                                                     |
| CCS811 conditioning window    | enforced for the first 20 min after boot                             | [`app/src/sensor_ccs811_driver.c`](../app/src/sensor_ccs811_driver.c)                               |
| CCS811 baseline persistence   | NVS write every 24 h                                                 | [`app/src/sensor_ccs811_driver.c`](../app/src/sensor_ccs811_driver.c)                               |
| MPU / microphone power rails  | held off (HIL-verified)                                              | [`app/tests/hardware/src/test_hil_power_rails.c`](../app/tests/hardware/src/test_hil_power_rails.c) |

Numerical interpretations of the symbolic Kconfig / Zephyr macros (e.g. what
microseconds `BT_GAP_ADV_SLOW_INT_MIN` evaluates to, or which dBm value
`CONFIG_BT_CTLR_TX_PWR_0` selects on this SoC) must be cross-checked against
the Zephyr source actually built into the firmware before being repeated here.

## Sampling policy (connection-driven)

[`sensor_manager`](../app/src/sensor_manager.c) runs two independent work
items:

- `env_work` — HTS221, LPS22HB, battery ADC. Started by
  `sensor_manager_on_connected()`, cancelled by
  `sensor_manager_on_disconnected()` when `connected_count` falls to zero.
  No I²C traffic on these sensors while purely advertising.
- `aq_work` — CCS811. Started by `sensor_manager_arm()` and never stopped.
  The CCS811 must keep running to preserve its conditioning state and 24 h
  baseline.

On every new connection the manager runs an env read **before** the AQ read so
the CCS811 environmental compensation uses fresh temp/humidity, and the first
ESS notify after subscribe carries data no older than the I²C round-trip time.

## Connected vs disconnected behaviour

| State                           | HTS221 / LPS22HB / battery                                                               | CCS811                    |
| ------------------------------- | ---------------------------------------------------------------------------------------- | ------------------------- |
| Disconnected (advertising only) | not sampled (`env_work` cancelled)                                                       | sampled at base × divisor |
| Connected                       | sampled every `CONFIG_SENSOR_ENV_INTERVAL_SEC`                                           | unchanged                 |
| Connect transition              | one immediate env read, then one immediate AQ read using the just-refreshed compensation | —                         |
| Disconnect transition           | `env_work` cancelled when `connected_count == 0`                                         | unchanged                 |

## Per-state current

> Intentionally empty. See the policy at the top of this file.

To populate this section, add the following primary sources to the repo (or
link to a controlled internal location) and cite the exact page / table for
every number:

- nRF52832 Product Specification (sleep current, advertising current vs
  TX power and PHY, SAADC active current, NVS write energy).
- HTS221 datasheet (typical / max active current, conversion time at the
  configured ODR, power-down current).
- LPS22HB datasheet (one-shot conversion current and time, power-down
  current).
- CCS811 datasheet (drive-mode current vs measurement period, wake / sleep
  transitions, I²C standby current).
- SX1509B datasheet (quiescent current, GPIO drive while holding MPU /
  MIC / CCS power gates).
- Thingy:52 reference schematic (PCA20020) — battery path, voltage-divider
  values for `vbatt`, regulator quiescent current, leakage paths.

For dynamic numbers (advertising and connected-event current at the configured
interval and TX power, energy per env / AQ sample, energy per CCS811 NVS save),
the only acceptable source is a Power Profiler Kit II capture on this hardware
running this firmware. Attach the raw `.ppk` and screenshots alongside the
table.

## What is *not yet* measured

- Sleep current with all configured Zephyr subsystems running.
- Average / peak advertising current at the configured interval and TX power.
- Energy per HTS221 / LPS22HB / SAADC read burst.
- Energy per CCS811 measurement at its production drive mode.
- Energy per CCS811 NVS baseline write (every 24 h).
- Battery life on the on-board cell with default Kconfig values.
