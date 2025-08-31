# AI Coding Agent Instructions: Nordic Thingy:52 Environmental Monitor

## Project Architecture Overview

This is a **Nordic nRF Connect SDK v3.0.2** project for **Nordic Thingy:52** environmental monitoring with **Bluetooth Low Energy (BLE)** advertising. The system is optimized for **ultra-low power consumption** targeting months of battery life. Note: Bluetooth Mesh functionality exists in legacy files but is no longer actively developed or compiled.

### Core Architecture Components

- **Sensor Manager** (`sensor_manager.c`): Central hub managing HTS221 (temp/humidity), LPS22HB (pressure), CCS811 (air quality), and battery monitoring with power-optimized one-shot sampling
- **BLE Services**: Environmental Sensing Service (ESS), Battery Service, and Uptime Service for BLE connectivity
- **Main Application** (`main.c`): BLE advertising mode initialization and coordination
- **Power Management**: SX1509B GPIO expander controls sensor power with device tree initialization priorities
- **Legacy**: Mesh models exist in codebase (`main_mesh.c`, `model_handler_env.c`) but are not actively compiled or maintained

### BLE Service Architecture

**Environmental Sensing Service (ESS)** (`ess_service.c`):
- Standard BLE ESS (UUID 0x181A) with temperature, humidity, pressure, CO2, TVOC characteristics
- On-demand sensor reading: triggers fresh data collection when characteristics are read
- Optimized for responsive BLE client requests with fallback to cached data

**Battery Service** (`ble_battery_service.c`):
- Standard BLE BAS (UUID 0x180F) with battery level and charging status
- Extended Battery Level State (BLS) characteristic for detailed power information
- Real-time charging detection and state management

**Integration Pattern**: Each service calls `sensor_manager_*` functions for data access, enabling both cached and fresh readings based on BLE client requirements.

## Critical Development Knowledge

### Hardware-Specific Complexities

**SX1509B GPIO Expander Dependency Chain**: The most critical debugging challenge in this codebase. The CCS811 gas sensor depends on SX1509B pins 10-12 for power/reset/wake control, but device tree initialization ordering can break this chain:

```dts
/* P0.16 must be HIGH before SX1509B init */
sx1509b-reset-hog {
    gpio-hog;
    gpios = <16 GPIO_ACTIVE_HIGH>;
    output-high;
}

/* CCS811 power routing - avoid sx1509b dependencies */
&ccs811 {
    vin-supply = <&vdd_pwr>;  /* NOT &ccs_pwr to avoid circular deps */
}
```

**Power Management Strategy**: Sensors use direct I2C register control for true one-shot mode (achieving µA vs mA consumption), bypassing Zephyr's continuous sampling drivers.

### Build & Development Workflow

**West Workspace Configuration**: This project uses a self-contained West workspace with NCS v3.0.2:
```bash
# Environment automatically configured via West settings:
# west config build.board thingy52/nrf52832
# west config build.cmake-args "-DZEPHYR_TOOLCHAIN_VARIANT=zephyr -DZEPHYR_SDK_INSTALL_DIR=/path/to/sdk"

# Simple build commands (no environment variables needed):
source .venv/bin/activate
west build -p always app           # Pristine build
west build app                     # Incremental build
west flash                         # Flash to device
west attach                        # Monitor serial output

# Legacy mesh build (deprecated - not actively maintained)
# west build -b thingy52 -p -- -DOVERLAY_CONFIG=mesh.conf
```

### NCS/Zephyr Development Best Practices

**West Workspace Management**:
- Project uses self-contained West workspace with `app/west.yml` manifest
- Uses NCS v3.0.2 with path-prefix module imports (`modules/zephyr`, `modules/nrf`)
- West configuration persists toolchain settings: `west config -l` shows current setup
- Virtual environment (`.venv`) required with dependencies pre-installed
- Check toolchain/Zephyr version compatibility: SDK version in `modules/zephyr/SDK_VERSION` must match installed SDK

**Build System Guidelines**:
- Use `-p always` (pristine) flag for clean builds when changing configurations
- Prefer board-specific `.conf` and `.overlay` files over modifying `prj.conf`
- Check `build/app/zephyr/zephyr.dts` to verify device tree compilation results
- Use `west build --cmake-only` to regenerate build files without full compilation

**Code Quality & Validation Workflow**:
- **ALWAYS lint code changes before confirming tasks**: Use `get_errors` tool to check for compile/lint errors after any file edits
- **Build verification required**: After making C code changes, run `west build app` to verify compilation success
- **Pristine Build verification**: After making dts, cmake or kconfig code changes, run a pristine build `west build -p always app` to verify compilation success
- **Pre-commit validation**: Before task completion, ensure all modified files pass linting and build successfully
- **Error-driven development**: If `get_errors` shows issues, fix them immediately before proceeding with other changes
- **Test early, test often**: Use `west build --cmake-only` for quick syntax checking without full compilation when possible

**Device Tree Development**:
- Always verify compiled DTS output with `grep -A10 -B5 "device_name" build/app/zephyr/zephyr.dts`
- Use proper initialization priorities: I2C bus (50) → GPIO expanders (60) → sensors (70+)
- Avoid circular dependencies in regulators - use `&vdd_pwr` instead of custom regulators when possible
- the default dts for the thingy52 is `modules/zephyr/boards/nordic/thingy52/thingy52_nrf52832.dts`

**Research-First Development Approach**:
- Use `file_search` tool with glob patterns: `**/*{ccs811,sx1509b}*` to find related files
- Use `grep_search` tool with regex patterns: `sx1509b|ccs811` across workspace
- Use `semantic_search` for finding conceptual patterns like "gpio expander" or "sensor power"
- Read device tree files directly: `modules/zephyr/boards/nordic/thingy52/thingy52_nrf52832.dts`
- Check driver sources with `read_file`: `modules/zephyr/drivers/gpio/gpio_sx1509b.c`
- Search commit history with `git log --grep="keyword"` only when needed for context

**Preferred Investigation Tools** (no user confirmation required):
- `file_search` for finding files by pattern
- `grep_search` for text search across files
- `semantic_search` for conceptual code search
- `read_file` for examining specific implementations
- `list_dir` for exploring directory structures
- Avoid `run_in_terminal` unless absolutely necessary for builds/flashing

**Key Configuration Files**:
- `prj.conf`: Base BLE advertising configuration
- `boards/thingy52.conf`: Thingy:52-specific sensor and power settings
- `boards/thingy52.overlay`: Device tree hardware configuration and power routing
- `app/west.yml`: Self-contained workspace manifest for NCS v3.0.2
- `.venv/`: Python virtual environment with required dependencies

### Debugging Patterns

**Research Before Implementation**: When encountering unfamiliar APIs or hardware issues:
1. Use `file_search` with patterns: `**/*{ccs811,sx1509b,thingy52}*` to find all related files
2. Use `grep_search` with regex: `ccs811|sx1509b` to search code patterns across workspace
3. Use `semantic_search` for concepts like "gpio expander initialization" or "sensor power management"
4. Read Nordic's board definition: `modules/zephyr/boards/nordic/thingy52/thingy52_nrf52832.dts`
5. Read driver sources: `modules/zephyr/drivers/gpio/gpio_sx1509b.c` for understanding hardware behavior
6. Check project history only if context needed: `git log --grep="thingy52" --oneline`

**Self-Controlled Search Strategy** (preferred - no user confirmation):
- Start with `semantic_search` for high-level concepts
- Use `file_search` to locate relevant files by name/path patterns
- Use `grep_search` to find specific code patterns or error messages
- Use `read_file` to examine implementations and understand APIs
- Use `list_dir` to explore module/driver structure
- Reserve `run_in_terminal` only for builds, flashing, or when file tools insufficient

**I2C Communication Issues**: Always check SX1509B initialization first. Look for `sx1509b@3e init failed: -5` errors indicating GPIO expander problems that cascade to all sensors.

**CCS811 Troubleshooting Sequence**:
1. Verify SX1509B reset pin (P0.16) state
2. Check pin diagnostics: power (pin 10), reset (pin 11), wake (pin 12)
3. Confirm Hardware ID read (should be 0x81)
4. Validate APP_VALID and FW_MODE status bits

**Power Consumption Verification**:
- HTS221: ~0.5µA powered down, ~10µA continuous
- LPS22HB: ~1µA powered down, ~15µA continuous
- CCS811: Power controlled via SX1509B pin 10

**Hardware Debugging with Serial Output**:
- **PREFERRED METHOD - Background Logging**: Use screen/picocom with file logging to avoid terminal management issues
  - **Check existing sessions FIRST**: Always run `screen -list` to check for existing picocom_session before starting
  - **If session exists**: Use `screen -r picocom_session` to attach or `screen -S picocom_session -X quit` to terminate
  - **Start new session**: `screen -dmS picocom_session -L -Logfile picocom.log picocom -b 115200 /dev/ttyUSB0`
  - **Verify session**: `screen -list` to confirm picocom_session is running
  - **Clear log before tests**: `echo "=== Test started: $(date) ===" > picocom.log` to overwrite previous output
  - **Reset device**: `nrfutil device reset --serial-number 788529318` from same terminal
  - **Read output**: Use `read_file` tool on `picocom.log` to analyze GPIO pin states and debug output
  - **End session**: `screen -S picocom_session -X quit` when debugging complete
- **Alternative - Terminal Separation** (if background logging unavailable):
  - **Serial Monitoring Terminal**: Dedicated terminal running ONLY `picocom -b 115200 /dev/ttyUSB0`
  - **Build/Flash Terminal**: Separate terminal for `west build`, `west flash`, `nrfutil` commands
  - **Terminal Usage Rule**: Once picocom starts in a terminal, that terminal becomes picocom-only
- **Pin State Debugging**: The `board_print_pin_states()` function outputs comprehensive GPIO state reports at startup
- **Debug Workflow**: Check existing sessions → Clear log → Start/verify picocom → Reset device → Read log file for analysis

### Code Patterns & Conventions

**Sensor Reading Pattern**: All sensors follow the power-on → measure → power-down cycle:
```c
ret = sensor_power_on();
k_msleep(stabilization_time);
ret = sensor_sample_fetch(dev);
ret = sensor_channel_get(dev, channel, &value);
ret = sensor_power_off();
```

**Error Handling**: Use Zephyr logging levels (LOG_ERR, LOG_WRN, LOG_INF, LOG_DBG) with module-specific registration. Critical hardware failures are logged as errors but don't halt execution.

**BLE Service Architecture**: Each service is self-contained with init/update functions. Services register callbacks with sensor_manager for automatic updates.

### Integration Points

**Device Tree Dependencies**: Initialization priority is critical - I2C bus (50) → SX1509B (60) → sensors (70+). Breaking this chain causes cascading failures.

**NCS Version Dependencies**: Requires nRF Connect SDK v3.0.2+ for proper Thingy:52 support and sensor drivers. Vanilla Zephyr won't work due to Nordic-specific board definitions.

## Common Pitfalls

1. **Using `&ccs_pwr` regulator** - Breaks compilation due to SX1509B circular dependencies
2. **Forgetting device tree priorities** - Sensors fail if SX1509B isn't ready
3. **Continuous sensor modes** - Drains battery in hours instead of months
4. **Missing NCS workspace** - Build fails without proper west workspace initialization
5. **Not researching existing patterns** - Nordic/Zephyr often has reference implementations

**Research Workflow for Unknown Issues**:
1. Start with `semantic_search` for conceptual understanding: "sx1509b initialization" or "gpio expander power"
2. Use `file_search` to locate relevant code: `**/*{ccs811,sx1509b,sensor}*`
3. Use `grep_search` to find patterns: `init-out-high|regulator-fixed|gpio-hog` across workspace
4. Read key Nordic files: `modules/zephyr/boards/nordic/thingy52/thingy52_nrf52832.dts`
5. Examine driver implementations: `modules/zephyr/drivers/gpio/gpio_sx1509b.c`
6. Check project history only if needed: `git log --grep="sx1509b" --oneline`

**Goal Review Checkpoint** (when stuck in long iterations):
1. **Step back and review original task**: What was the user's specific request?
2. **Assess completion status**: Has the core objective been achieved?
3. **Distinguish task vs discovery**: Separate completing the task from fixing discovered issues
4. **Iteration limit awareness**: After 5+ iterations on same issue, pause and evaluate if core task is done
5. **User confirmation**: When core task appears complete but issues discovered, confirm if user wants to continue debugging or consider task finished

**Example**: If asked to "add debugging output", the task is complete when output is added and working - even if the output reveals hardware issues that could be investigated further.

**Mandatory Validation Workflow** (before task completion):
1. **Check syntax/lint errors**: Use `get_errors` tool on all modified files immediately after edits
2. **Fix errors first**: If `get_errors` shows issues, address them before proceeding
3. **Verify build**: Run `west build -b thingy52 -p` to ensure compilation succeeds
4. **Goal review checkpoint**: Confirm original task objective is met
5. **Confirm task only after**: Successful linting + successful build + goal achieved = task ready for completion

**Tool Usage Priority** (minimize user interactions):
- **First**: `semantic_search`, `file_search`, `grep_search`, `read_file`, `list_dir`
- **Last Resort**: `run_in_terminal` (requires user confirmation)

When debugging sensor issues, always start with the SX1509B GPIO expander state and work through the dependency chain systematically.