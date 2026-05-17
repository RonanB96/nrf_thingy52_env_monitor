# Developer Guide — nRF Thingy:52 Environment Monitor

This guide covers everything needed to contribute to or extend this firmware beyond
basic build and flash. See [README.md](README.md) for the end-user quick-start.

---

## Contents

- [Environment Setup](#environment-setup)
- [Project Structure](#project-structure)
- [Code Style and Formatting](#code-style-and-formatting)
- [Static Analysis](#static-analysis)
- [Testing](#testing)
- [AI Agent Hardware Interaction](#ai-agent-hardware-interaction)
- [CI Pipeline](#ci-pipeline)
- [Contributing Workflow](#contributing-workflow)
- [Power Budget](docs/low_power.md)

---

## Environment Setup

The sections below cover the minimum required for a working build.

### Prerequisites

- Linux Ubuntu 22.04+ or macOS
- Python 3.10+
- nRF Connect SDK v3.x toolchain — install via
  [Toolchain Manager](https://www.nordicsemi.com/Products/Development-tools/nrf-connect-for-desktop)
  (recommended) or manually:

```bash
sudo apt update
sudo apt install --no-install-recommends git cmake ninja-build gperf \
  ccache dfu-util device-tree-compiler wget python3-dev python3-venv \
  xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1
```

- `clang-format` and `clang-tidy` for linting. The repository `.clang-format` is a symlink to
  `modules/zephyr/.clang-format`, so local formatting always tracks Zephyr upstream style.

```bash
sudo apt install clang-format-14 clang-tidy
sudo ln -sf /usr/bin/clang-format-14 /usr/local/bin/clang-format
```

### Python Virtual Environment

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip west
west packages pip --install
```

### Per-shell environment activation

Run these in every new terminal before building firmware:

```bash
source .venv/bin/activate
source modules/zephyr/zephyr-env.sh
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
export ZEPHYR_SDK_INSTALL_DIR="$(west config zephyr.sdk-install-dir)"
```

### Toolchain path (if not auto-detected)

```bash
west config --local zephyr.sdk-install-dir <absolute-path-to-zephyr-sdk>
```

### Build verification

After running the per-shell environment activation snippet above:

```bash
west build -p always app -d app/build          # Pristine build
west build app -d app/build                   # Incremental build
```

## Project Structure

```
app/
├── CMakeLists.txt                # Application CMake entry point
├── Kconfig                       # Application Kconfig symbols
├── prj.conf                      # Base project configuration
├── boards/                       # Board-specific .conf and .overlay files
│   ├── thingy52.conf             # Thingy:52 Kconfig overrides
│   └── thingy52.overlay          # Thingy:52 devicetree overlay
├── include/                      # Public header files
│   ├── sensor_manager.h
│   ├── ess_service.h
│   ├── battery_service.h
│   └── ...
├── src/                          # Application source files
│   ├── main.c                    # Application init and BLE startup
│   ├── sensor_manager.c          # Periodic sampling and aggregation
│   ├── sensor_ccs811_driver.c    # CCS811 driver and baseline persistence
│   ├── ess_service.c             # Environmental Sensing Service (ESS)
│   ├── battery_service.c         # Battery Service
│   └── ...
└── tests/                        # Unit tests (Zephyr ZTEST, native_sim)
    ├── CMakeLists.txt
    ├── prj.conf
    ├── testcase.yaml
    └── src/
        ├── test_main.c
        ├── test_sensor_manager.c
        ├── test_ess_service.c
        ├── test_battery_service.c
        └── mocks/
```

Architecture layers (innermost first):

1. **Sensor drivers** (`sensor_*_driver.c`) — talk directly to hardware via Zephyr sensor API
2. **Sensor manager** (`sensor_manager.c`) — orchestrates periodic sampling and data storage
3. **BLE services** (`ess_service.c`, `battery_service.c`, `uptime_service.c`) — read from manager,
   update GATT characteristics
4. **Main** (`main.c`) — initialises all subsystems and arms periodic sampling

---

## Code Style and Formatting

### Style Rules

This project follows the
[Zephyr C coding style](https://docs.zephyrproject.org/latest/contribute/coding_guidelines/index.html)
and the
[Linux kernel coding style](https://kernel.org/doc/html/latest/process/coding-style.html)

A [`.clang-format`](.clang-format) file at the repository root enforces the above rules.

### Formatting a File

```bash
clang-format -i app/src/<file>.c
```

### Formatting All Source Files (including tests)

```bash
find app/src app/include app/tests/src -name '*.c' -o -name '*.h' | xargs clang-format -i
```

### Checking Without Modifying

```bash
find app/src app/include app/tests/src -name '*.c' -o -name '*.h' \
  | xargs clang-format --dry-run --Werror
```

A non-zero exit code means at least one file needs formatting. Fix with the commands above.

---

## Static Analysis

### CodeChecker (recommended for NCS/Zephyr)

Use CodeChecker with the repository configuration files:

- `.codechecker.json` (analyzer configuration)
- `.codechecker.skip` (scope filter)

This setup analyses only this project's application sources and skips Zephyr/module code.

Build once to generate/update `compile_commands.json`:

After running the per-shell environment activation snippet above:

```bash
west build -p always app -d app/build
```

Run CodeChecker without rebuilding:

```bash
CodeChecker analyze -o .codechecker_reports \
  --config .codechecker.json \
  app/build/app/compile_commands.json
CodeChecker parse --config .codechecker.json .codechecker_reports
```

The `.clang-tidy` file remains the source of checks (`clang-tidy:take-config-from-directory=true` is set in `.codechecker.json`).

---

## Testing

Tests live under `app/tests/` and use the
[Zephyr ZTEST framework](https://docs.zephyrproject.org/latest/develop/test/ztest.html).
Two test targets exist:

| Target | Board | Purpose |
|--------|-------|---------|
| `app/tests/` | `native_sim` | Unit tests with mocks — no hardware required |
| `app/tests/hardware/` | `thingy52/nrf52832` | HIL tests — exercises real sensor drivers on device |

### Running Tests

```bash
source .venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=host
west build app/tests -d app/tests/build -b native_sim -p always
west build -d app/tests/build -t run
```

Or run the compiled binary directly:

```bash
./app/build/tests/zephyr/zephyr.exe
```

A successful run ends with:

```
PROJECT EXECUTION SUCCESSFUL
```

### Running Tests via Twister

[Twister](https://docs.zephyrproject.org/latest/develop/test/twister.html) is Zephyr's
test-runner harness. It reads `app/tests/testcase.yaml` to discover and execute tests.

```bash
source .venv/bin/activate
./modules/zephyr/scripts/twister \
  -T app/tests \
  -p native_sim \
  --inline-logs
```

Results are written to `twister-out/`. Pass `--report-dir <dir>` to customise the output
location.

### Hardware-in-the-Loop (HIL) Tests

HIL tests live under `app/tests/hardware/` and run the ZTEST framework directly on the
Thingy:52 using real sensor hardware. Twister flashes the binary and reads `PROJECT EXECUTION
SUCCESSFUL` from the UART to determine pass/fail.

**Build and flash manually:**

```bash
source .venv/bin/activate && source env.sh
west build app/tests/hardware -d app/tests/hardware/build -b thingy52/nrf52832 -p always
west flash -d app/tests/hardware/build --runner jlink
```

**Run via Twister with the hardware map:**

```bash
source .venv/bin/activate && source env.sh
./modules/zephyr/scripts/twister \
  -T app/tests/hardware \
  --hardware-map hardware.map \
  --device-testing \
  --inline-logs
```

### Writing a New Test

Test files follow the same Zephyr C coding style as application sources:

- Same `clang-format` and `clang-tidy` rules apply (`HeaderFilterRegex` in `.clang-tidy`
  covers `app/tests/src/`)

```c
#include <zephyr/ztest.h>

ZTEST_SUITE(my_suite, NULL, NULL, NULL, NULL, NULL);

ZTEST(my_suite, test_example)
{
	zassert_equal(1 + 1, 2, "arithmetic is broken");
}
```

Add the file to `app/tests/CMakeLists.txt` under `target_sources`.

### Code Coverage

Twister wires Zephyr's [native_sim coverage flow](https://docs.zephyrproject.org/latest/develop/test/coverage.html#coverage-reports-using-the-posix-architecture)
end-to-end: pass `--coverage` and it sets `CONFIG_COVERAGE=y` for every
test image, runs them, and post-processes the resulting `.gcda`/`.gcno`
files with the tool of your choice
([Twister docs](https://docs.zephyrproject.org/latest/develop/test/coverage.html#coverage-reports-using-twister)).

#### One-command flow

The `Coverage Report` VS Code task (or the equivalent commands below)
runs the full suite and renders an **app-only** HTML report that includes
an explicit audit of which `app/src/*.c` files are not exercised by any
native_sim test:

```bash
source .venv/bin/activate && source env.sh
rm -rf twister-out
./modules/zephyr/scripts/twister \
  -T app/tests \
  -p native_sim \
  --coverage \
  --coverage-tool lcov \
  --coverage-basedir "$PWD" \
  --coverage-formats html \
  --inline-logs

./scripts/coverage_report.sh
# open twister-out/coverage_app/index.html
```

`--coverage-basedir "$PWD"` is required to avoid path-mismatch drops
in the merged tracefile (Zephyr issue
[#83764](https://github.com/zephyrproject-rtos/zephyr/issues/83764), fixed
in v4.1.0). Twister must be invoked from the workspace root.

#### What the report contains

`scripts/coverage_report.sh` filters `twister-out/coverage.info` down to
`app/src/*` and `app/include/*`, renders HTML at
`twister-out/coverage_app/index.html`, and prints a list of `app/src/*.c`
files that were not linked into any native_sim test image. Those files
are explicitly *out of scope* for native_sim coverage and must rely on
the HIL suite under `app/tests/hardware/` for verification.

#### Files exercised on native_sim

The native_sim coverage path covers the application logic that does not
depend on the Nordic SoC HAL or BLE controller binary. The thin sensor
wrapper layer (`sensor_hts221_driver.c`, `sensor_lps22hb_driver.c`,
`sensor_ccs811_driver.c`) is exercised by an integration test under
`app/tests/integration/full_app/` that runs the upstream Zephyr sensor
drivers against minimal in-tree I²C emul backends in `emul/`. The
backends are deliberately register-table-only — they answer just enough
traffic for the upstream drivers to pass `init` and report not-ready on
data — see `app/tests/integration/full_app/emul/hts221_emul.c` for the
documented rationale.

#### Files NOT covered by native_sim

The following are HIL-only and will appear in the audit list:

| File | Reason |
|------|--------|
| `app/src/board.c` | Direct Nordic HAL access (`NRF_P0`, `nrf_gpio_pin_dir_get`); will not link or run on native_sim. |
| `app/src/main.c` | System bring-up; covered by smoke tests on real hardware. |
| `app/src/ble_advertiser.c` | BLE advertising stack; would need a HCI controller emul fixture not in scope. |
| `app/src/battery_service.c` | ADC + voltage-divider backend; depends on Nordic SAADC. |

These files are covered by `app/tests/hardware/` (HIL) which runs
pass/fail on `thingy52/nrf52832` without coverage instrumentation
(nRF52832's 64 KB RAM is below Zephyr's documented threshold for
on-device gcov).

> Requires `lcov` ≥ 1.14 (intermediate text format support, per the
> Zephyr coverage docs).

---

## AI Agent Hardware Interaction

This section documents how an AI agent (or any automated process) controls the
hardware without human involvement.

### Hardware inventory

| Item | Value |
|------|-------|
| Board | thingy52/nrf52832 |
| Debugger | J-Link Ultra (S/N 505103055) |
| Serial port | `/dev/ttyUSB[X]` at 115200 baud |
| Hardware map | `hardware.map` (Twister format) |

Confirm the hardware is reachable:

```bash
nrfutil device list
ls -la /dev/ttyUSB[X]
```

### Flash firmware

Always activate the venv and source `env.sh` first:

```bash
source .venv/bin/activate
source env.sh
west flash -d app/build --runner jlink
```

### Capture boot logs

The device only emits serial output during the first ~1.5 seconds after reset.
The logger **must be running before flash is triggered** to avoid missing boot
output.  Use `scripts/serial_logger.py` — it stays open until explicitly killed:

**Terminal 1 — logger (start first):**
```bash
python3 scripts/serial_logger.py /dev/ttyUSB[X] 115200 boot.log
```

**Terminal 2 — flash:**
```bash
source .venv/bin/activate && source env.sh
west flash -d app/build --runner jlink
```

Once the expected log lines have been captured, kill the logger (`Ctrl-C` or
`kill <pid>`).  Do **not** add a time-based deadline to the logger — it must
remain open for as long as needed and be closed explicitly.

### Full verification (automated)

`scripts/verify_hardware.sh` orchestrates both steps and asserts three boot
patterns:

```bash
bash scripts/verify_hardware.sh [build_dir]
```

Expected output:

```
=== Thingy:52 Hardware Verification ===
[1/4] Checking JLink...
  OK: JLink detected
[2/4] Checking serial port /dev/ttyUSB1...
  OK: /dev/ttyUSB1 accessible
[3/4] Starting serial logger then flashing...
  OK: west flash succeeded
[4/4] Verifying boot log...
  OK: 'Booting BLE Env Monitor'
  OK: 'sensor_manager: Sensor manager initialized'
  OK: 'ble_advertiser: Legacy advertising started successfully'

=== PASS: Hardware verification complete ===
```

### Twister hardware testing

The `hardware.map` file at the repository root describes the attached device
in [Twister hardware-map format](https://docs.zephyrproject.org/latest/develop/test/twister.html#hardware-testing):

```yaml
- connected: true
  id: '505103055'
  platform: thingy52/nrf52832
  runner: jlink
  serial: /dev/ttyUSB[X]
  baud: 115200
```

To run hardware tests via Twister:

```bash
source .venv/bin/activate && source env.sh
./modules/zephyr/scripts/twister \
  -T app/tests/hardware \
  --hardware-map hardware.map \
  --device-testing \
  --inline-logs
```

### Permissions

The current user must be in the `dialout` group to access `/dev/ttyUSB[X]`:

```bash
sudo usermod -aG dialout $USER   # then log out and back in
```

---

## CI Pipeline

The CI pipeline uses three workflows running in the Zephyr public CI container
(`ghcr.io/zephyrproject-rtos/ci-repo-cache:v0.27.4.20241026`):

1. [.github/workflows/ci-static-analysis.yml](.github/workflows/ci-static-analysis.yml)
2. [.github/workflows/ci-build.yml](.github/workflows/ci-build.yml)
3. [.github/workflows/ci-unit-tests.yml](.github/workflows/ci-unit-tests.yml)

`ci-static-analysis.yml` runs static checks only:

1. `format-and-lint` (`clang-format`, `checkpatch`, `yamllint`, `actionlint`)
2. `codechecker` (static analysis)

`ci-build.yml` runs the standard firmware build.

`ci-unit-tests.yml` runs native simulation unit tests.

To replicate CI locally before pushing:

```bash
source .venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=host
west build app/tests -d app/tests/build -b native_sim -p always
./app/tests/build/zephyr/zephyr.exe
```

---

## Contributing Workflow

1. Branch from `develop`: `git checkout -b feature/<short-description>`
2. Keep changes narrowly scoped — one logical change per commit
3. Format all touched files before committing (see [Code Style](#code-style-and-formatting))
4. Run tests locally and confirm they pass before opening a pull request
5. Open a pull request against `develop`; CI must pass before merge

### Pristine vs. Incremental Builds

- Use `west build -p always app` after any change to `Kconfig`, `.conf`, `.overlay`, or
  `CMakeLists.txt`
- Use `west build app` for C-only source changes

### Board-Specific Configuration

Board `.conf` files under `app/boards/` contain only overrides from `prj.conf`. If a
setting must apply to all boards, put it in `prj.conf`.

### Commit Message Style

```
subsystem: short description in imperative mood

Optional longer explanation of why, not what. Wrap at 72 characters.
```

Examples:

```
sensor: move air quality divisor to Kconfig
ess: add TVOC notification support
battery: fix signed overflow in voltage calculation
```
