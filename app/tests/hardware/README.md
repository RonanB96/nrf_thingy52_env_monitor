# Hardware-in-the-Loop (HIL) Testing Infrastructure for Thingy:52

This directory contains the complete Hardware-in-the-Loop testing infrastructure for the Nordic Thingy:52 Environmental Monitor project.

## Overview

The HIL testing framework provides comprehensive validation of real hardware functionality including:

- ✅ **Sensor Hardware Validation**: Real I2C communication with HTS221, LPS22HB, CCS811 sensors
- ✅ **GPIO Expander Testing**: SX1509B dependency chain validation (critical for CCS811)
- ✅ **BLE Connectivity Testing**: Service discovery, characteristic access, mobile app compatibility
- ✅ **Power Consumption Validation**: Baseline power, sensor optimization, sleep modes
- ✅ **Automated Test Execution**: Python scripts for complete test automation
- ✅ **Hardware Debug Output**: GPIO pin state validation and system diagnostics

## Directory Structure

```
app/tests/hardware/
├── main_hardware_test.c          # Main test firmware entry point
├── test_hardware_sensors.c       # Environmental sensor validation tests
├── test_hardware_gpio.c          # GPIO expander and pin state tests
├── test_hardware_ble.c           # BLE connectivity and service tests
├── test_hardware_power.c         # Power consumption validation tests
├── hardware_test.h               # Test function declarations
├── CMakeLists.txt                # Build configuration for hardware tests
├── prj.conf                      # Hardware test firmware configuration
├── fixtures/
│   └── hardware_test_reference.md # Reference values and test criteria
└── README.md                     # This file

scripts/
├── hardware_test_runner.py       # Complete automated test execution
└── ble_test_automation.py        # Dedicated BLE testing with Python/Bleak
```

## Quick Start

### Prerequisites

1. **Hardware Setup**:
   - Nordic Thingy:52 device
   - USB connection for flashing and serial monitoring
   - nRF Connect mobile app for BLE testing (optional)
   - Nordic Power Profiler Kit II for power measurements (optional)

2. **Software Requirements**:
   ```bash
   # Python dependencies
   pip install pyserial bleak
   
   # Nordic nRF Connect SDK v3.0.2 environment activated
   source .venv/bin/activate  # Or your NCS environment
   ```

### Running Hardware Tests

#### Option 1: Automated Test Suite (Recommended)

```bash
# Run complete automated test suite
cd /path/to/nrf_thingy52_env_monitor
python scripts/hardware_test_runner.py

# With specific device serial
python scripts/hardware_test_runner.py --device-serial 788529318

# Build firmware only
python scripts/hardware_test_runner.py --build-only

# Monitor existing test session
python scripts/hardware_test_runner.py --monitor-only
```

#### Option 2: Manual Build and Flash

```bash
# Build hardware test firmware
cd app/tests/hardware
west build -b thingy52 -p always .

# Flash to device
west flash

# Monitor serial output
picocom -b 115200 /dev/ttyUSB0
# OR
west attach
```

#### Option 3: BLE Testing Only

```bash
# Scan for Thingy:52 devices
python scripts/ble_test_automation.py --scan-only

# Test specific device
python scripts/ble_test_automation.py --address EB:XX:XX:XX:XX:XX

# Automated BLE validation
python scripts/ble_test_automation.py
```

## Test Categories

### 1. Sensor Hardware Tests (`test_hardware_sensors.c`)

**What it tests**:
- HTS221 temperature/humidity sensor I2C communication
- LPS22HB pressure sensor with interrupt functionality
- CCS811 air quality sensor (conditioning awareness)
- Sensor power cycling and optimization

**Expected results**:
- All sensors respond to I2C communication
- Temperature: -10°C to +60°C range
- Humidity: 0% to 100% RH range
- Pressure: 800hPa to 1200hPa range
- CCS811: May show "conditioning" for new sensors (24-48 hours)

### 2. GPIO Expander Tests (`test_hardware_gpio.c`)

**What it tests**:
- SX1509B GPIO expander initialization sequence
- CCS811 power control dependency chain (pins 10-12)
- Device tree initialization order validation
- GPIO pin state debugging infrastructure

**Critical dependencies**:
- P0.16 (SX1509B reset) must be HIGH
- I2C bus → GPIO0 → SX1509B → CCS811 initialization order
- SX1509B pins 10 (power), 11 (reset), 12 (wake) control CCS811

### 3. BLE Hardware Tests (`test_hardware_ble.c`)

**What it tests**:
- BLE advertising visibility and timing
- Service discovery (ESS, Battery, Uptime services)
- Characteristic read functionality
- Connection stability

**Manual validation**:
- Use nRF Connect mobile app to scan and connect
- Verify Environmental Sensing Service (0x181A)
- Read temperature, humidity, pressure characteristics
- Check Battery Service (0x180F)

### 4. Power Consumption Tests (`test_hardware_power.c`)

**What it tests**:
- Baseline power consumption measurement
- Sensor power cycling effectiveness
- Sleep mode power usage
- Battery monitoring accuracy

**Expected power ranges**:
- Sleep mode: <50µA
- BLE advertising: 100-500µA
- Sensor active: <10mA peak
- Target battery life: 6+ months

## Hardware Debug Features

### GPIO Pin State Debugging

The firmware includes comprehensive GPIO debugging via `board_print_pin_states()`:

```
=== Thingy:52 GPIO Pin State Report ===
GPIO0 pins (nRF52832):
  Pin 16 (SX1509B_RESET): HIGH ✓
  Pin 7 (I2C_SDA): configured
  Pin 8 (I2C_SCL): configured
  
SX1509B pins (GPIO Expander):
  Pin 10 (CCS811_PWR): HIGH
  Pin 11 (CCS811_RESET): HIGH  
  Pin 12 (CCS811_WAKE): LOW
```

### Serial Monitoring

Hardware test firmware provides detailed logging:

```
✓ PASS: HTS221 Device Ready
✓ PASS: HTS221 Temperature Read (23.45°C)
✓ PASS: LPS22HB Pressure Read (1013.2hPa)
⚠ CCS811 still conditioning - normal for new sensors
✓ PASS: BLE Advertising Started
```

## Test Result Validation

### Pass Criteria

- **All sensors respond to I2C communication**
- **SX1509B GPIO expander initializes successfully**
- **BLE advertising starts and is discoverable**
- **Power consumption within specified limits**
- **Environmental readings within plausible ranges**

### Warning Conditions (Not Failures)

- **CCS811 in conditioning mode** (new sensors need 24-48 hours)
- **Environmental readings outside office range** (extreme conditions OK)
- **Power consumption elevated but within limits**

### Failure Conditions

- **Sensor I2C communication failures**
- **SX1509B initialization failure** (check P0.16 reset pin)
- **BLE advertising fails to start**
- **Power consumption exceeds maximum limits**
- **Sensor readings clearly invalid** (impossible values)

## Troubleshooting

### Common Issues

#### SX1509B Initialization Failure
```
✗ FAIL: SX1509B Device Ready - Device not ready
```
**Solution**: Check P0.16 reset pin state, verify device tree initialization order

#### CCS811 Not Ready
```
⚠ CCS811 still conditioning - normal for new sensors
```
**Solution**: Normal for new sensors, wait 24-48 hours or test other sensors

#### BLE Advertising Issues
```
✗ FAIL: BLE Start Advertising - Start advertising failed
```
**Solution**: Check BLE configuration, device conflicts, or reset device

#### Power Measurements
```
⚠ External power measurement recommended
```
**Solution**: Use Nordic Power Profiler Kit II for accurate measurements

### Hardware Debugging Workflow

1. **Check GPIO pin states**:
   ```
   board_print_pin_states(); // In firmware
   ```

2. **Verify device initialization order**:
   - I2C bus (priority 50)
   - GPIO0 (priority 40) 
   - SX1509B (priority 60)
   - Sensors (priority 70+)

3. **Test individual components**:
   ```bash
   # Test specific sensor
   python scripts/hardware_test_runner.py --monitor-only
   # Look for specific sensor test results
   ```

4. **BLE debugging**:
   ```bash
   # Scan for device
   python scripts/ble_test_automation.py --scan-only
   
   # Test specific device
   python scripts/ble_test_automation.py --address EB:XX:XX:XX:XX:XX
   ```

## Integration with CI/CD

The HIL infrastructure can be integrated into CI/CD pipelines:

```yaml
# Example GitHub Actions workflow (requires hardware runner)
- name: Run Hardware Tests
  run: |
    python scripts/hardware_test_runner.py --device-serial ${{ secrets.DEVICE_SERIAL }}
    
- name: Upload Test Results
  uses: actions/upload-artifact@v3
  with:
    name: hardware-test-results
    path: hardware_test_report_*.txt
```

## External Tool Integration

### Nordic nRF Connect Mobile App

1. Install nRF Connect from app store
2. Scan for "Thingy:52" or custom device name
3. Connect and discover services
4. Read characteristics to validate sensor data
5. Test notifications if enabled

### Nordic Power Profiler Kit II (PPK2)

1. Connect PPK2 to Thingy:52 VDD rail
2. Use nRF Connect for Desktop Power Profiler app
3. Measure current during test execution
4. Validate against expected power ranges

### Serial Console Tools

```bash
# picocom (recommended)
picocom -b 115200 /dev/ttyUSB0

# screen with logging
screen -dmS thingy_test -L -Logfile test.log picocom -b 115200 /dev/ttyUSB0

# west attach
west attach
```

## Contributing

When adding new hardware tests:

1. Follow existing test patterns in `test_hardware_*.c`
2. Use `record_*_test_result()` functions for consistent reporting
3. Add test documentation to `fixtures/hardware_test_reference.md`
4. Update this README with new test descriptions
5. Test with real hardware before submitting

## Reference Documentation

- [Thingy:52 Hardware Reference](../../../modules/zephyr/boards/nordic/thingy52/)
- [nRF Connect SDK Testing Guide](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/test_and_optimize/testing.html)
- [Project Architecture](../../../.github/copilot-instructions.md)
- [Development Setup](../../../.github/DEVELOPMENT_SETUP.md)