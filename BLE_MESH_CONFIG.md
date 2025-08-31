# BLE vs Mesh Configuration Guide

This project supports both BLE advertising and Bluetooth Mesh configurations for environmental monitoring.

## Current Configuration: BLE Advertising

The project is now configured for **BLE Advertising** mode, which provides:
- Simple BLE advertisements with sensor data
- Ultra-low power consumption
- No mesh networking complexity
- Direct sensor data in advertisement packets

## Files for Each Mode

### BLE Advertising Mode (Current)

- **Main source**: `src/main.c`
- **Configuration**: `prj.conf` + `boards/thingy52.conf`
- **Sysbuild IPC**: `sysbuild/ipc_radio/prj.conf`
- **BLE Advertiser**: `src/ble_advertiser.c` + `include/ble_advertiser.h`

### Bluetooth Mesh Mode (Backed up)

- **Main source**: `src/main_mesh.c` (backed up)
- **Mesh handler**: `src/model_handler_env.c` + `include/model_handler.h`
- **Mesh configuration**: `mesh.conf` + `boards/thingy52_mesh.conf`
- **Sysbuild IPC**: `sysbuild/ipc_radio/prj_mesh.conf` (backed up)

## Switching Between Modes

### To Enable BLE Advertising (Current)
```bash
# Already configured - just build normally
west build --board thingy52 app
```

### To Enable Bluetooth Mesh

1. **Swap main files**:
   ```bash
   cd app/src
   mv main.c main_ble.c
   mv main_mesh.c main.c
   ```

2. **Restore sysbuild configuration**:
   ```bash
   cd app/sysbuild/ipc_radio
   mv prj.conf prj_ble.conf
   mv prj_mesh.conf prj.conf
   ```

3. **Update CMakeLists.txt**:
   ```cmake
   target_sources(app PRIVATE
       src/main.c
       src/sensor_manager.c
       src/model_handler_env.c  # Add this line
       src/battery_service.c
   )
   ```

4. **Update configuration files**:
   ```bash
   # Add mesh configurations to prj.conf
   cat mesh.conf >> prj.conf

   # Add mesh board config
   cat boards/thingy52_mesh.conf >> boards/thingy52.conf
   ```

5. **Build with mesh**:
   ```bash
   west build --board thingy52 app --pristine
   ```

## Architecture

### Common Components
- **Sensor Manager** (`sensor_manager.c/.h`): Unified sensor interface
- **Battery Service** (`battery_service.c/.h`): Battery monitoring
- **Hardware drivers**: HTS221, LPS22HB, CCS811 sensors

### BLE Advertising Specific
- **BLE Advertiser** (`ble_advertiser.c/.h`): Manages BLE advertisements
- **Main** (`main.c`): Simple initialization and periodic updates

### Mesh Specific
- **Model Handler** (`model_handler_env.c/.h`): BLE Mesh sensor server
- **Main** (`main_mesh.c`): Mesh provisioning and initialization

## Power Consumption

### BLE Advertising Mode
- **Advertising interval**: 1-1.28 seconds
- **Sensor update**: 30 minutes
- **Expected battery life**: 6+ months

### Mesh Mode
- **LPN poll timeout**: 4+ minutes
- **Sensor update**: 30 minutes
- **Expected battery life**: 3-6 months (depends on mesh activity)

## Benefits of Each Mode

### BLE Advertising
✅ Simpler implementation
✅ Lower power consumption
✅ No mesh provisioning required
✅ Works with any BLE scanner
❌ No mesh networking features
❌ Limited range (single hop)

### Bluetooth Mesh
✅ Multi-hop networking
✅ Mesh reliability
✅ Standardized sensor models
✅ Advanced provisioning security
❌ More complex setup
❌ Higher power consumption
❌ Requires mesh infrastructure
