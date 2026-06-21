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
| Advertising interval          | 1.0 … 2.56 s (`BT_GAP_ADV_SLOW_INT_MIN` … `BT_GAP_SCAN_SLOW_INTERVAL_2`) | [`app/src/ble_advertiser.c`](../app/src/ble_advertiser.c) — longer max lowers adv duty cycle   |
| TX power Kconfig              | `CONFIG_BT_CTLR_TX_PWR_0=y`                                          | [`app/prj.conf`](../app/prj.conf)                                                                   |
| LE Privacy (RPA)              | disabled (static random address from HW ID)                          | [`app/prj.conf`](../app/prj.conf) (`CONFIG_BT_CTLR_PRIVACY=n`)                                      |
| Connection interval (PPCP)    | 4000 ms, latency 80, timeout 655 s (Kconfig max)                   | [`app/boards/thingy52.conf`](../app/boards/thingy52.conf) — central may negotiate differently  |
| Periodic connected sampling   | off by default (`CONFIG_SENSOR_PERIODIC_SAMPLING=n`)                 | [`app/Kconfig`](../app/Kconfig)                                                                     |
| Env sample period (optional)  | `CONFIG_SENSOR_ENV_INTERVAL_SEC` (900 s when periodic enabled)       | [`app/Kconfig`](../app/Kconfig)                                                                     |
| AQ sample period (optional)   | base × `CONFIG_SENSOR_AIR_QUALITY_DIVISOR` when periodic enabled     | [`app/Kconfig`](../app/Kconfig)                                                                     |
| Sampling while disconnected   | one boot sample only (cache seed)                                    | [`app/src/sensor_manager.c`](../app/src/sensor_manager.c)                                           |
| CCS811 conditioning window    | 20 min from first GATT connect (`ccs811_driver_begin_sampling_session`) | [`app/src/sensor_ccs811_driver.c`](../app/src/sensor_ccs811_driver.c)                    |
| CCS811 baseline persistence   | NVS write every 24 h (only during periodic AQ work)                   | [`app/src/sensor_ccs811_driver.c`](../app/src/sensor_ccs811_driver.c)                               |
| MPU / microphone power rails  | held off (HIL-verified)                                              | [`app/tests/hardware/src/test_hil_power_rails.c`](../app/tests/hardware/src/test_hil_power_rails.c) |

Numerical interpretations of the symbolic Kconfig / Zephyr macros (e.g. what
microseconds `BT_GAP_ADV_SLOW_INT_MIN` evaluates to, or which dBm value
`CONFIG_BT_CTLR_TX_PWR_0` selects on this SoC) must be cross-checked against
the Zephyr source actually built into the firmware before being repeated here.

## Sampling policy (default: boot + connect)

[`sensor_manager`](../app/src/sensor_manager.c) and
[`ble_battery_service`](../app/src/ble_battery_service.c) read sensors **once at
boot** and **once on each GATT connect** by default. Interval-based re-reads
while connected require `CONFIG_SENSOR_PERIODIC_SAMPLING=y`.

- `sensor_manager_init()` — one full sensor read at boot.
- `sensor_manager_on_connected()` — immediate env sample, then AQ sample.
- `CONFIG_SENSOR_PERIODIC_SAMPLING` — when enabled, starts `env_work`,
  `aq_work`, and BAS poll loops at the configured intervals until disconnect.
- `ble_battery_service_on_connected()` — one BAS hardware read on connect;
  periodic BAS poll only when `CONFIG_SENSOR_PERIODIC_SAMPLING=y`.

On every connect the manager runs env **before** AQ so CCS811 compensation uses
fresh temp/humidity.

## Connected vs disconnected behaviour (default config)

| State                           | HTS221 / LPS22HB / battery | CCS811 | BAS ADC reads |
| ------------------------------- | -------------------------- | ------ | ------------- |
| Boot                            | one sample                 | one sample | seeded from boot sample |
| Disconnected (advertising)      | no further reads           | no further reads | no reads |
| Connect transition              | immediate env sample       | immediate AQ after env | one read on connect |
| Connected (default)             | no periodic reads          | no periodic reads | no periodic reads |
| Connected (`PERIODIC_SAMPLING`) | every `CONFIG_SENSOR_ENV_INTERVAL_SEC` | every base × divisor | same interval |
| Disconnect                      | —                          | —      | poll cancelled (if periodic) |

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
- Energy per CCS811 NVS baseline save (every 24 h).
- Battery life on the on-board cell with default Kconfig values.
