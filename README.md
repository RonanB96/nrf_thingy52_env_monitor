# nRF Thingy:52 Environment Monitor

Bluetooth Low Energy environmental monitor for the Nordic Thingy:52. Reads temperature,
humidity, pressure, CO₂, TVOC, and battery level using a low-power sampling strategy and
exposes all data over standard Bluetooth SIG GATT services.

## Hardware Requirements

- Nordic Thingy:52 (PCA20020)
- USB cable for programming and power

## Software Requirements

- nRF Connect SDK-compatible toolchain (NCS v3.x) — install via
  [nRF Connect Toolchain Manager](https://www.nordicsemi.com/Products/Development-tools/nrf-connect-for-desktop)
  or follow the [Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)
- Python 3.10+ with venv support
- west (installed below)

## Quick Start

This repository is a self-contained west workspace.

```bash
git clone https://github.com/RonanB96/nrf_thingy52_env_monitor.git
cd nrf_thingy52_env_monitor
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip west
west packages pip --install
```

Set up the firmware build shell (run in each new terminal):

```bash
# Check your .west/config files points correctly to your SDK install if you have multiple versions.
source env.sh
```

Build:

```bash
west build -p always app -d app/build
```

Flash and monitor:

```bash
west flash -d app/build
west attach -d app/build
```

Daily build after the first run:

```bash
source .venv/bin/activate
# Run the firmware build shell setup block from Quick Start first.
west build app -d app/build
```

> The repository includes workspace defaults in [.west/config](.west/config) (board:
> `thingy52/nrf52832`, pristine policy: `auto`). For machine-specific SDK path overrides:
> `west config --local zephyr.sdk-install-dir <path>`

## Features

- Temperature and humidity sensing (HTS221)
- Pressure sensing (LPS22HB)
- Air quality sensing (CCS811 — CO₂ / TVOC)
- Battery level and charging status
- Environmental Sensing Service (ESS) GATT characteristics
- Low-power periodic sampling with configurable intervals

## BLE Services

| Service | UUID |
|---|---|
| Environmental Sensing Service | 0x181A |
| Battery Service | 0x180F |
| Uptime Service | custom |

Values are readable from any BLE GATT client, including Nordic nRF Connect (mobile and desktop).

## GATT Characteristic Reference

| Sensor | UUID | Service |
|---|---|---|
| Temperature | 0x2A6E | 0x181A |
| Humidity | 0x2A6F | 0x181A |
| Pressure | 0x2A6D | 0x181A |
| CO₂ Concentration | 0x2B8C | 0x181A |
| TVOC Concentration | 0x2BE7 | 0x181A |
| Battery Level | 0x2A19 | 0x180F |

> Note: 0x2BD0 is Carbon Monoxide (CO), not CO₂. CO₂ Concentration is 0x2B8C.

## Troubleshooting

- Activate `.venv` and source `modules/zephyr/zephyr-env.sh` before any `west` or `CodeChecker` command.
- Ensure `ZEPHYR_TOOLCHAIN_VARIANT=zephyr` for firmware builds.
- After Kconfig or devicetree changes, run a pristine build: `west build -p always app`.
- If SDK auto-detection fails, set `zephyr.sdk-install-dir` in local west config (see Quick Start above).
- Check serial logs with `west attach` for sensor and BLE startup diagnostics.

## Developer Documentation

See [DEVELOPER.md](DEVELOPER.md) for full environment setup, code style, linting, static
analysis, and testing instructions.

## Licence

Licensed under the Nordic 5-Clause Licence.
