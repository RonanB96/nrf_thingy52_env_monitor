# Hardware Test Reference Values and Fixtures
# This file contains reference values for hardware validation

## Environmental Sensor Reference Ranges

### HTS221 Temperature/Humidity Sensor
- **Temperature Range**: -10°C to +60°C (indoor testing range)
- **Humidity Range**: 0% to 100% RH
- **Accuracy**: ±0.5°C, ±3.5% RH (typical)
- **Expected Power**: <10µA active, <1µA standby

### LPS22HB Pressure Sensor  
- **Pressure Range**: 800hPa to 1200hPa (atmospheric range)
- **Accuracy**: ±1hPa (typical)
- **Expected Power**: <15µA active, <1µA standby

### CCS811 Air Quality Sensor
- **eCO2 Range**: 400ppm to 8192ppm
- **TVOC Range**: 0ppb to 1187ppb
- **Conditioning Period**: 24-48 hours for new sensors
- **Expected Power**: <30mA active (via SX1509B control)

## GPIO Expander Reference Values

### SX1509B GPIO Expander
- **I2C Address**: 0x3E
- **Reset Pin**: P0.16 (must be HIGH)
- **CCS811 Control Pins**: 10 (power), 11 (reset), 12 (wake)
- **Initialization Priority**: 60 (after I2C bus at 50)

## Power Consumption Reference Values

### Target Power Consumption
- **Sleep Mode**: <50µA
- **BLE Advertising**: 100-500µA average
- **Sensor Active**: <10mA peak
- **Baseline**: <100µA with BLE + sensors off

### Battery Life Targets
- **220mAh Coin Cell**: 6+ months typical operation
- **Sleep Only**: 12+ months
- **Continuous Operation**: 3+ months

## BLE Service Reference UUIDs

### Standard Services
- **Environmental Sensing Service**: 0x181A
- **Battery Service**: 0x180F

### Custom Services  
- **Uptime Service**: 01234567-89AB-CDEF-0123-456789ABCDEF

### Characteristic Types
- **Temperature**: 0x2A6E (int16, 0.01°C resolution)
- **Humidity**: 0x2A6F (uint16, 0.01% resolution)  
- **Pressure**: 0x2A6D (uint32, 0.1Pa resolution)
- **CO2 Concentration**: 0x2A6D (uint16, 1ppm resolution)

## Test Environment Specifications

### Expected Ambient Conditions
- **Temperature**: 18°C to 25°C (office environment)
- **Humidity**: 30% to 60% RH (comfortable indoor)
- **Pressure**: 950hPa to 1050hPa (sea level ±100m)
- **Air Quality**: 400-1000ppm CO2 (indoor typical)

### Hardware Validation Equipment
- **Power Measurement**: Nordic Power Profiler Kit II (PPK2)
- **BLE Testing**: nRF Connect mobile app
- **Environmental Reference**: Calibrated sensors or known conditions
- **Serial Monitoring**: 115200 baud UART console

## Test Pass/Fail Criteria

### Critical Pass Requirements
1. **All sensors respond to I2C communication**
2. **SX1509B GPIO expander initializes successfully**
3. **BLE advertising starts and is discoverable**
4. **Power consumption within specified limits**
5. **Environmental readings within plausible ranges**

### Warning Conditions (Not Failures)
1. **CCS811 in conditioning mode** (new sensors)
2. **Environmental readings outside typical office range** (extreme conditions)
3. **Power consumption elevated but within limits**
4. **BLE connection instability** (mobile device specific)

### Failure Conditions
1. **Sensor I2C communication failures**
2. **SX1509B initialization failure**
3. **BLE advertising fails to start**
4. **Power consumption exceeds maximum limits**
5. **Sensor readings clearly invalid** (impossible values)