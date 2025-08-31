# nRF Thingy:52 Environment Monitor

This project creates a **Bluetooth Low Energy (BLE) environmental sensor** using the Nordic Thingy:52 device for ultra-low power room monitoring. The device advertises environmental data via BLE and provides GATT services for real-time sensor access.

## Hardware Requirements

- Nordic Thingy:52 (PCA20020)
- USB cable for programming and power

## Software Requirements

- nRF Connect SDK v2.7.0 or later
- west tool
- nRF Connect for Desktop (for device programming)

## Setup Instructions

### 1. Install nRF Connect SDK

First, install the nRF Connect SDK following the [official installation guide](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/installation.html).

### 2. Initialize the workspace

```bash
# Create a workspace directory
mkdir ncs-workspace
cd ncs-workspace

# Initialize west with NCS
west init -m https://github.com/nrfconnect/sdk-nrf --mr v2.7.0

# Update dependencies
west update

# Install Python dependencies
pip3 install -r zephyr/scripts/requirements.txt
pip3 install -r nrf/scripts/requirements.txt
pip3 install -r bootloader/mcuboot/scripts/requirements.txt
```

### 3. Clone this application

```bash
# Clone to the workspace
git clone https://github.com/your-username/nrf_thingy_env_monitor.git

# Or copy your existing code to the workspace
```

### 4. Build the application

```bash
# Navigate to your application directory
cd nrf_thingy_env_monitor

# Build for Thingy:52 (standard build)
west build -b thingy52 -p

# Flash to the device
west flash
```

## Features

- **Temperature and Humidity Sensing**: Using HTS221 sensor with power-optimized sampling
- **Pressure Sensing**: Using LPS22HB sensor with interrupt-driven measurements
- **Air Quality Monitoring**: Using CCS811 gas sensor for CO2/TVOC detection
- **Battery Service**: Real-time battery level and charging status monitoring
- **Bluetooth Low Energy**: Environmental Sensing Service (ESS) with GATT characteristics
- **Ultra Low Power Operation**: Optimized for months of battery life
  - One-shot sensor readings with power-down between measurements
  - Direct I2C register control for µA-level power consumption
  - Configurable sampling intervals

## BLE Services & Integration

The device provides standard Bluetooth Low Energy GATT services:

- **Environmental Sensing Service (ESS)**: Temperature, humidity, pressure, CO2, and TVOC characteristics
- **Battery Service**: Battery level percentage and charging status
- **Uptime Service**: Device uptime for monitoring reliability

### Mobile App Integration

Use any BLE scanner app or build custom applications using:

- Nordic's nRF Connect mobile app for testing
- Standard ESS service UUIDs for environmental data
- Custom characteristic UUIDs for extended functionality

### Home Assistant Integration

For Home Assistant integration, use:

1. **ESPHome Bluetooth Proxy** on ESP32 devices
2. **Bluetooth integration** with passive BLE monitor
3. **Custom component** for ESS service parsing

### ESPHome BLE Proxy Integration (Recommended)

**This is the recommended approach for reliable Home Assistant integration.** Use an ESP32 device running ESPHome as a BLE proxy to automatically discover and parse all Thingy:52 sensors into standard Home Assistant entities.

#### ESP32 Setup

Create an ESPHome configuration file (`thingy52-proxy.yaml`):

```yaml
esphome:
  name: thingy52-proxy
  friendly_name: "Thingy52 BLE Proxy"

esp32:
  board: esp32dev
  framework:
    type: arduino

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:
  encryption:
    key: !secret api_encryption_key

ota:
  - platform: esphome
    password: !secret ota_password

logger:

# Enable Bluetooth proxy
esp32_ble_tracker:
  scan_parameters:
    interval: 1100ms
    window: 1000ms
    active: true

bluetooth_proxy:
  active: true

# Auto-discover Thingy52 devices and create sensors
sensor:
  # Kitchen Thingy52 (replace with your device's MAC address)
  - platform: ble_client
    mac_address: "AA:BB:CC:DD:EE:FF"  # Your Thingy52-XXXXXXXX MAC
    name: "Kitchen"

    # Temperature (ESS Service - Standard Bluetooth SIG UUID)
    temperature:
      service_uuid: "181A"                    # Environmental Sensing Service
      characteristic_uuid: "2A6E"             # Temperature Characteristic
      name: "Kitchen Temperature"
      unit_of_measurement: "°C"
      accuracy_decimals: 1
      device_class: "temperature"
      state_class: "measurement"
      # ESS uses IEEE-11073 format: int16 * 0.01°C
      filters:
        - lambda: |-
            if (x.size() >= 2) {
              int16_t temp_raw = (x[1] << 8) | x[0];  // Little-endian
              return temp_raw * 0.01;                  // Convert to °C
            }
            return {};

    # Humidity (ESS Service - Standard Bluetooth SIG UUID)
    humidity:
      service_uuid: "181A"                    # Environmental Sensing Service
      characteristic_uuid: "2A6F"             # Humidity Characteristic
      name: "Kitchen Humidity"
      unit_of_measurement: "%"
      accuracy_decimals: 1
      device_class: "humidity"
      state_class: "measurement"
      # ESS uses uint16 * 0.01%
      filters:
        - lambda: |-
            if (x.size() >= 2) {
              uint16_t humidity_raw = (x[1] << 8) | x[0];  // Little-endian
              return humidity_raw * 0.01;                   // Convert to %
            }
            return {};

    # Pressure (ESS Service - Standard Bluetooth SIG UUID)
    pressure:
      service_uuid: "181A"                    # Environmental Sensing Service
      characteristic_uuid: "2A6D"             # Pressure Characteristic
      name: "Kitchen Pressure"
      unit_of_measurement: "hPa"
      accuracy_decimals: 1
      device_class: "atmospheric_pressure"
      state_class: "measurement"
      # ESS uses uint32 * 0.1 Pa, convert to hPa
      filters:
        - lambda: |-
            if (x.size() >= 4) {
              uint32_t pressure_raw = (x[3] << 24) | (x[2] << 16) | (x[1] << 8) | x[0];
              return (pressure_raw * 0.1) / 100.0;  // Convert Pa to hPa
            }
            return {};

    # Air Quality - CO2 (Custom Bluetooth SIG Assigned UUID)
    co2:
      service_uuid: "181A"                    # Environmental Sensing Service
      characteristic_uuid: "2BD0"             # CO2 Concentration (Bluetooth SIG assigned)
      name: "Kitchen CO2"
      unit_of_measurement: "ppm"
      accuracy_decimals: 0
      device_class: "carbon_dioxide"
      state_class: "measurement"
      # uint16 direct ppm value
      filters:
        - lambda: |-
            if (x.size() >= 2) {
              uint16_t co2_raw = (x[1] << 8) | x[0];  // Little-endian
              return co2_raw;                          // Direct ppm value
            }
            return {};

    # Air Quality - TVOC (Custom Bluetooth SIG Assigned UUID)
    tvoc:
      service_uuid: "181A"                    # Environmental Sensing Service
      characteristic_uuid: "2BE7"             # TVOC Concentration (Bluetooth SIG assigned)
      name: "Kitchen TVOC"
      unit_of_measurement: "ppb"
      accuracy_decimals: 0
      state_class: "measurement"
      # uint16 direct ppb value
      filters:
        - lambda: |-
            if (x.size() >= 2) {
              uint16_t tvoc_raw = (x[1] << 8) | x[0];  // Little-endian
              return tvoc_raw;                          // Direct ppb value
            }
            return {};

    # Battery Level (Battery Service - Standard Bluetooth SIG UUID)
    battery_level:
      service_uuid: "180F"                    # Battery Service
      characteristic_uuid: "2A19"             # Battery Level
      name: "Kitchen Sensor Battery"
      unit_of_measurement: "%"
      device_class: "battery"
      state_class: "measurement"
      # uint8 direct percentage value (0-100)
      filters:
        - lambda: |-
            if (x.size() >= 1) {
              return x[0];  // Direct percentage value
            }
            return {};

binary_sensor:
  # Battery charging status (Battery Level Service - Extended)
  - platform: ble_client
    mac_address: "AA:BB:CC:DD:EE:FF"
    name: "Kitchen Sensor Charging"
    service_uuid: "180F"                      # Battery Service
    characteristic_uuid: "2A1B"               # Battery Power State (BLS)
    device_class: "battery_charging"
    # Decode Battery Level Service power state
    filters:
      - lambda: |-
          if (x.size() >= 1) {
            uint8_t state = x[0];
            // Check charging bit (varies by implementation)
            return (state & 0x01) != 0;  // Bit 0: charging state
          }
          return false;

#### ESPHome Package Template (Recommended for Multiple Devices)

**Yes, you can and should create a template!** ESPHome supports packages for reusable configurations:

**Create `thingy52-package.yaml`:**
```yaml
# thingy52-package.yaml - Reusable template for Thingy52 sensors
sensor:
  - platform: ble_client
    mac_address: !lambda "return \"${device_mac}\";"
    name: "${room_name}"

    # All sensor definitions with room name substitution
    temperature:
      service_uuid: "181A"
      characteristic_uuid: "2A6E"
      name: "${room_name} Temperature"
      unit_of_measurement: "°C"
      accuracy_decimals: 1
      device_class: "temperature"
      state_class: "measurement"
      filters:
        - lambda: |-
            if (x.size() >= 2) {
              int16_t temp_raw = (x[1] << 8) | x[0];
              return temp_raw * 0.01;
            }
            return {};

    humidity:
      service_uuid: "181A"
      characteristic_uuid: "2A6F"
      name: "${room_name} Humidity"
      unit_of_measurement: "%"
      accuracy_decimals: 1
      device_class: "humidity"
      state_class: "measurement"
      filters:
        - lambda: |-
            if (x.size() >= 2) {
              uint16_t humidity_raw = (x[1] << 8) | x[0];
              return humidity_raw * 0.01;
            }
            return {};

    pressure:
      service_uuid: "181A"
      characteristic_uuid: "2A6D"
      name: "${room_name} Pressure"
      unit_of_measurement: "hPa"
      accuracy_decimals: 1
      device_class: "atmospheric_pressure"
      state_class: "measurement"
      filters:
        - lambda: |-
            if (x.size() >= 4) {
              uint32_t pressure_raw = (x[3] << 24) | (x[2] << 16) | (x[1] << 8) | x[0];
              return (pressure_raw * 0.1) / 100.0;
            }
            return {};

    co2:
      service_uuid: "181A"
      characteristic_uuid: "2BD0"
      name: "${room_name} CO2"
      unit_of_measurement: "ppm"
      accuracy_decimals: 0
      device_class: "carbon_dioxide"
      state_class: "measurement"
      filters:
        - lambda: |-
            if (x.size() >= 2) {
              uint16_t co2_raw = (x[1] << 8) | x[0];
              return co2_raw;
            }
            return {};

    tvoc:
      service_uuid: "181A"
      characteristic_uuid: "2BE7"
      name: "${room_name} TVOC"
      unit_of_measurement: "ppb"
      accuracy_decimals: 0
      state_class: "measurement"
      filters:
        - lambda: |-
            if (x.size() >= 2) {
              uint16_t tvoc_raw = (x[1] << 8) | x[0];
              return tvoc_raw;
            }
            return {};

    battery_level:
      service_uuid: "180F"
      characteristic_uuid: "2A19"
      name: "${room_name} Sensor Battery"
      unit_of_measurement: "%"
      device_class: "battery"
      state_class: "measurement"
      filters:
        - lambda: |-
            if (x.size() >= 1) {
              return x[0];
            }
            return {};

binary_sensor:
  - platform: ble_client
    mac_address: !lambda "return \"${device_mac}\";"
    name: "${room_name} Sensor Charging"
    service_uuid: "180F"
    characteristic_uuid: "2A1B"
    device_class: "battery_charging"
    filters:
      - lambda: |-
          if (x.size() >= 1) {
            uint8_t state = x[0];
            return (state & 0x01) != 0;
          }
          return false;
```

**Use the template in device-specific configs:**
```yaml
# kitchen-thingy52.yaml
substitutions:
  room_name: "Kitchen"
  device_mac: "AA:BB:CC:DD:EE:FF"  # Your actual Kitchen Thingy52 MAC

esphome:
  name: kitchen-thingy52
  includes:
    - thingy52-package.yaml

packages:
  thingy52: !include thingy52-package.yaml

# Rest of your ESP32 config...
esp32:
  board: esp32dev

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:
bluetooth_proxy:
  active: true
```

**Benefits of Template Approach:**

✅ **DRY Principle**: Write sensor config once, reuse everywhere
✅ **Easy Updates**: Change package file to update all devices
✅ **Type Safety**: Substitutions ensure consistent naming
✅ **Scalable**: Add new rooms by creating simple config files
✅ **Maintainable**: Single source of truth for sensor definitions
```

#### Multi-Room Deployment Strategy

1. **Single ESP32 Proxy Per Floor/Area**: One ESP32 can handle multiple Thingy52 devices within BLE range (~10-30 meters)

2. **Device Mapping**: Create a mapping table for your devices:
   ```
   Room           | Thingy52 Name     | MAC Address       | ESP32 Proxy
   Kitchen        | Thingy52-A1B2C3D4 | AA:BB:CC:DD:EE:FF | esp32-main-floor
   Living Room    | Thingy52-5E6F7A8B | 11:22:33:44:55:66 | esp32-main-floor
   Master Bedroom | Thingy52-C7D8E9FA | 99:88:77:66:55:44 | esp32-upper-floor
   ```

3. **Home Assistant Dashboard**: All sensors automatically appear as standard entities:
   ```yaml
   # Example Lovelace card
   type: entities
   title: Environmental Monitoring
   entities:
     - sensor.kitchen_temperature
     - sensor.kitchen_humidity
     - sensor.kitchen_pressure
     - sensor.kitchen_co2
     - sensor.kitchen_tvoc
     - sensor.kitchen_sensor_battery
     - binary_sensor.kitchen_sensor_charging
   ```

#### Benefits of ESPHome Proxy Approach

✅ **Automatic Discovery**: ESP32 handles all BLE communication
✅ **Standard Entities**: All sensors appear as native Home Assistant entities
✅ **Reliable Connection**: ESP32 maintains persistent BLE connections
✅ **Easy Scaling**: Add more Thingy52 devices by updating ESPHome config
✅ **Home Assistant Integration**: Full support for automations, history, dashboards
✅ **No Custom Components**: Uses standard ESPHome BLE client functionality

#### Advanced Configuration Examples

**Air Quality Monitoring with Alerts**:
```yaml
# ESPHome automation for air quality alerts
automation:
  - trigger:
      platform: numeric_state
      entity_id: sensor.kitchen_co2
      above: 1000
    action:
      - homeassistant.service:
          service: notify.mobile_app
          data:
            message: "High CO2 detected in kitchen: {{ states('sensor.kitchen_co2') }} ppm"

  - trigger:
      platform: numeric_state
      entity_id: sensor.kitchen_tvoc
      above: 500
    action:
      - homeassistant.service:
          service: notify.mobile_app
          data:
            message: "High TVOC detected in kitchen: {{ states('sensor.kitchen_tvoc') }} ppb"
```

**Multi-Device Template Sensors**:
```yaml
# Home Assistant configuration.yaml
template:
  - sensor:
      - name: "House Average Temperature"
        unit_of_measurement: "°C"
        device_class: "temperature"
        state: >
          {% set temps = [
            states('sensor.kitchen_temperature') | float(0),
            states('sensor.living_room_temperature') | float(0),
            states('sensor.bedroom_temperature') | float(0)
          ] %}
          {{ (temps | sum / temps | length) | round(1) }}

      - name: "House Average Humidity"
        unit_of_measurement: "%"
        device_class: "humidity"
        state: >
          {% set humidity = [
            states('sensor.kitchen_humidity') | float(0),
            states('sensor.living_room_humidity') | float(0),
            states('sensor.bedroom_humidity') | float(0)
          ] %}
          {{ (humidity | sum / humidity | length) | round(1) }}
```

#### Troubleshooting ESPHome Integration

**BLE Connection Issues**:
1. **Range**: Ensure ESP32 is within 10-30m of Thingy52 devices
2. **Interference**: Check for 2.4GHz interference (WiFi, microwave)
3. **ESP32 placement**: Position ESP32 centrally for best coverage
4. **Multiple proxies**: Use multiple ESP32s for large homes

**Sensor Data Not Updating**:
1. **Check MAC addresses**: Verify correct MAC addresses in ESPHome config
2. **Service UUIDs**: Confirm Thingy52 is advertising ESS service (0x181A)
3. **Battery level**: Low battery may affect BLE transmission
4. **Logs**: Check ESPHome logs for BLE connection status

**Performance Optimization**:
```yaml
# Optimize BLE scanning for better performance
esp32_ble_tracker:
  scan_parameters:
    interval: 320ms    # Faster scanning for quicker discovery
    window: 30ms       # Shorter window to reduce power consumption
    active: false      # Passive scanning for lower interference
```

## Power Consumption & Battery Life

The Thingy:52 has been optimized for maximum battery life with one-shot sensor readings:

### Expected Battery Life (with 1000mAh battery)

- **Standard Mode**: ~6-12 months
- **Optimized Mode**: ~12+ months with extended intervals

### Power Optimizations Applied

- One-shot sensor measurements with power-down between readings
- Direct I2C register control achieving µA vs mA consumption
- SX1509B GPIO expander power management
- Minimal BLE advertising intervals
- Automatic sensor power cycling

### Monitoring Battery Level

The device reports battery level and charging status through the Battery Service. Configure your BLE client to monitor battery alerts.

## Troubleshooting

### Build Issues

1. **Missing NCS**: Ensure you're building in an NCS workspace, not vanilla Zephyr
2. **Board not found**: Use `thingy52` as the board name (not `thingy52_nrf52832`)
3. **Missing dependencies**: Run `west update` to ensure all dependencies are installed
4. **SX1509B errors**: Check device tree overlay and GPIO expander initialization

### Runtime Issues

1. **Sensor not found**: Check that I2C is properly configured and SX1509B GPIO expander is working
2. **CCS811 not ready**: Verify SX1509B reset pin (P0.16) and power control (pin 10)
3. **BLE not advertising**: Ensure Bluetooth is properly initialized and no connection conflicts
4. **Power issues**: Connect USB cable or ensure battery is charged

### Hardware Debugging

Critical for CCS811 air quality sensor issues:

1. Check SX1509B initialization: Look for `sx1509b@3e init failed: -5` errors
2. Verify pin states: Power (pin 10), reset (pin 11), wake (pin 12)
3. Confirm I2C communication: Hardware ID should read 0x81
4. Check device tree priorities: I2C bus (50) → SX1509B (60) → sensors (70+)

## Development

The application is structured as follows:

- `app/src/main.c`: Main application initialization and BLE setup
- `app/src/sensor_manager.c`: Core sensor management with power optimization
- `app/src/ble_advertiser.c`: BLE advertising and connection management
- `app/src/ess_service.c`: Environmental Sensing Service implementation
- `app/src/battery_service.c`: Battery monitoring and service
- `app/include/`: Header files for all modules
- `app/prj.conf`: Main configuration file
- `app/boards/thingy52.conf`: Thingy:52 specific configuration
- `app/boards/thingy52.overlay`: Device tree hardware configuration

**Legacy Files** (not actively compiled):

- `mesh.conf`: Bluetooth Mesh configuration (deprecated)
- `main_mesh.c`: Mesh-specific implementation (deprecated)
- `model_handler_env.c`: Mesh model handlers (deprecated)

## License

This project is licensed under the Nordic 5-Clause License.
