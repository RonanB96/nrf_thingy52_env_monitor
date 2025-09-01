#!/usr/bin/env python3
"""
BLE Test Automation for Thingy:52 Environmental Monitor

This script provides automated BLE testing capabilities for HIL validation:
- Device discovery and connection
- Service and characteristic enumeration
- Data reading and validation
- Connection stability testing
- Mobile app simulation

Requirements:
- bleak (Python BLE library)
- asyncio for async BLE operations
"""

import asyncio
import sys
import argparse
import time
from typing import Optional, Dict, List, Any
from datetime import datetime

try:
    from bleak import BleakScanner, BleakClient
    from bleak.backends.characteristic import BleakGATTCharacteristic
    from bleak.backends.service import BleakGATTService
except ImportError:
    print("Error: bleak not installed. Run: pip install bleak")
    sys.exit(1)

class Thingy52BLETester:
    """Automated BLE testing for Thingy:52"""
    
    # Known service UUIDs
    ESS_SERVICE_UUID = "0000181a-0000-1000-8000-00805f9b34fb"
    BATTERY_SERVICE_UUID = "0000180f-0000-1000-8000-00805f9b34fb"
    
    # Known characteristic UUIDs
    TEMPERATURE_CHAR_UUID = "00002a6e-0000-1000-8000-00805f9b34fb"
    HUMIDITY_CHAR_UUID = "00002a6f-0000-1000-8000-00805f9b34fb"
    PRESSURE_CHAR_UUID = "00002a6d-0000-1000-8000-00805f9b34fb"
    BATTERY_LEVEL_UUID = "00002a19-0000-1000-8000-00805f9b34fb"
    
    def __init__(self):
        self.device_address: Optional[str] = None
        self.device_name: Optional[str] = None
        self.client: Optional[BleakClient] = None
        self.test_results: Dict[str, Any] = {}
        
    async def scan_for_devices(self, scan_time: int = 10) -> List[tuple]:
        """Scan for BLE devices and return Thingy:52 candidates"""
        print(f"Scanning for BLE devices for {scan_time} seconds...")
        
        devices = await BleakScanner.discover(timeout=scan_time)
        thingy_devices = []
        
        print(f"\nFound {len(devices)} BLE devices:")
        for device in devices:
            name = device.name or "Unknown"
            rssi = getattr(device, 'rssi', 'N/A')
            print(f"  {device.address} - {name} (RSSI: {rssi})")
            
            # Look for Thingy:52 devices
            if ("thingy" in name.lower() or 
                "nordic" in name.lower() or
                "env" in name.lower() or
                device.address.startswith("EB")):  # Nordic address prefix
                thingy_devices.append((device.address, name))
                print(f"    ↳ Potential Thingy:52 device!")
        
        return thingy_devices
    
    async def connect_to_device(self, address: str) -> bool:
        """Connect to specified BLE device"""
        print(f"Connecting to {address}...")
        
        try:
            self.client = BleakClient(address)
            await self.client.connect()
            
            if self.client.is_connected:
                print(f"✓ Connected to {address}")
                self.device_address = address
                return True
            else:
                print(f"✗ Failed to connect to {address}")
                return False
                
        except Exception as e:
            print(f"✗ Connection error: {e}")
            return False
    
    async def discover_services(self) -> Dict[str, Any]:
        """Discover and analyze BLE services"""
        if not self.client or not self.client.is_connected:
            print("✗ Not connected to device")
            return {}
        
        print("Discovering BLE services...")
        
        try:
            services = await self.client.get_services()
            service_info = {}
            
            print(f"Found {len(services.services)} services:")
            
            for service in services.services:
                uuid = str(service.uuid).lower()
                print(f"\nService: {service.uuid}")
                print(f"  Handle: {service.handle}")
                
                # Identify known services
                service_name = "Unknown"
                if self.ESS_SERVICE_UUID.replace("-", "").lower() in uuid:
                    service_name = "Environmental Sensing Service (ESS)"
                elif self.BATTERY_SERVICE_UUID.replace("-", "").lower() in uuid:
                    service_name = "Battery Service"
                elif len(uuid) > 8:  # Custom service (128-bit UUID)
                    service_name = "Custom Service (possibly Uptime)"
                
                print(f"  Type: {service_name}")
                
                # Discover characteristics
                char_info = []
                for char in service.characteristics:
                    char_uuid = str(char.uuid).lower()
                    properties = char.properties
                    
                    char_name = "Unknown"
                    if self.TEMPERATURE_CHAR_UUID.replace("-", "").lower() in char_uuid:
                        char_name = "Temperature"
                    elif self.HUMIDITY_CHAR_UUID.replace("-", "").lower() in char_uuid:
                        char_name = "Humidity"
                    elif self.PRESSURE_CHAR_UUID.replace("-", "").lower() in char_uuid:
                        char_name = "Pressure"
                    elif self.BATTERY_LEVEL_UUID.replace("-", "").lower() in char_uuid:
                        char_name = "Battery Level"
                    
                    print(f"    Characteristic: {char.uuid} ({char_name})")
                    print(f"      Properties: {properties}")
                    print(f"      Handle: {char.handle}")
                    
                    char_info.append({
                        'uuid': char.uuid,
                        'name': char_name,
                        'properties': properties,
                        'handle': char.handle
                    })
                
                service_info[service.uuid] = {
                    'name': service_name,
                    'handle': service.handle,
                    'characteristics': char_info
                }
            
            return service_info
            
        except Exception as e:
            print(f"✗ Service discovery error: {e}")
            return {}
    
    async def read_environmental_data(self) -> Dict[str, Any]:
        """Read environmental sensor data from characteristics"""
        if not self.client or not self.client.is_connected:
            return {}
        
        print("\nReading environmental sensor data...")
        sensor_data = {}
        
        try:
            # Read temperature
            try:
                temp_data = await self.client.read_gatt_char(self.TEMPERATURE_CHAR_UUID)
                if temp_data:
                    # Convert from BLE format (int16, 0.01°C resolution)
                    temp_raw = int.from_bytes(temp_data, byteorder='little', signed=True)
                    temperature = temp_raw / 100.0
                    sensor_data['temperature'] = temperature
                    print(f"  Temperature: {temperature:.2f}°C")
            except Exception as e:
                print(f"  Temperature read failed: {e}")
            
            # Read humidity
            try:
                humid_data = await self.client.read_gatt_char(self.HUMIDITY_CHAR_UUID)
                if humid_data:
                    # Convert from BLE format (uint16, 0.01% resolution)
                    humid_raw = int.from_bytes(humid_data, byteorder='little', signed=False)
                    humidity = humid_raw / 100.0
                    sensor_data['humidity'] = humidity
                    print(f"  Humidity: {humidity:.1f}%")
            except Exception as e:
                print(f"  Humidity read failed: {e}")
            
            # Read pressure
            try:
                press_data = await self.client.read_gatt_char(self.PRESSURE_CHAR_UUID)
                if press_data:
                    # Convert from BLE format (uint32, 0.1Pa resolution)
                    press_raw = int.from_bytes(press_data, byteorder='little', signed=False)
                    pressure = press_raw / 10.0 / 100.0  # Convert to hPa
                    sensor_data['pressure'] = pressure
                    print(f"  Pressure: {pressure:.1f}hPa")
            except Exception as e:
                print(f"  Pressure read failed: {e}")
            
            # Read battery level
            try:
                battery_data = await self.client.read_gatt_char(self.BATTERY_LEVEL_UUID)
                if battery_data:
                    battery_level = int.from_bytes(battery_data, byteorder='little', signed=False)
                    sensor_data['battery'] = battery_level
                    print(f"  Battery: {battery_level}%")
            except Exception as e:
                print(f"  Battery read failed: {e}")
            
        except Exception as e:
            print(f"✗ Error reading sensor data: {e}")
        
        return sensor_data
    
    async def test_connection_stability(self, duration: int = 30) -> bool:
        """Test BLE connection stability"""
        if not self.client or not self.client.is_connected:
            return False
        
        print(f"\nTesting connection stability for {duration} seconds...")
        
        start_time = time.time()
        successful_reads = 0
        failed_reads = 0
        
        while time.time() - start_time < duration:
            try:
                # Try to read a characteristic
                await self.client.read_gatt_char(self.BATTERY_LEVEL_UUID)
                successful_reads += 1
                print(f"  Read {successful_reads} successful", end='\r')
                
            except Exception as e:
                failed_reads += 1
                if not self.client.is_connected:
                    print(f"\n✗ Connection lost after {time.time() - start_time:.1f}s")
                    return False
            
            await asyncio.sleep(1)  # Test every second
        
        success_rate = (successful_reads / (successful_reads + failed_reads)) * 100
        print(f"\n✓ Connection stable: {successful_reads} successful, {failed_reads} failed")
        print(f"  Success rate: {success_rate:.1f}%")
        
        return success_rate > 90  # 90% success rate threshold
    
    async def disconnect(self):
        """Disconnect from BLE device"""
        if self.client and self.client.is_connected:
            await self.client.disconnect()
            print("✓ Disconnected from device")
    
    async def run_complete_ble_test(self, address: Optional[str] = None) -> Dict[str, Any]:
        """Run complete BLE test suite"""
        results = {
            'timestamp': datetime.now().isoformat(),
            'success': False,
            'tests': {}
        }
        
        try:
            # Step 1: Device discovery or direct connection
            if address:
                print(f"Testing specific device: {address}")
                results['tests']['connection'] = await self.connect_to_device(address)
            else:
                devices = await self.scan_for_devices()
                if not devices:
                    print("✗ No Thingy:52 devices found")
                    return results
                
                # Try to connect to first candidate
                address, name = devices[0]
                print(f"Testing device: {name} ({address})")
                results['tests']['connection'] = await self.connect_to_device(address)
            
            if not results['tests']['connection']:
                return results
            
            # Step 2: Service discovery
            services = await self.discover_services()
            results['services'] = services
            results['tests']['service_discovery'] = len(services) > 0
            
            # Check for expected services
            ess_found = any('181a' in str(uuid).lower() for uuid in services.keys())
            battery_found = any('180f' in str(uuid).lower() for uuid in services.keys())
            
            results['tests']['ess_service'] = ess_found
            results['tests']['battery_service'] = battery_found
            
            print(f"\nService validation:")
            print(f"  ESS Service: {'✓' if ess_found else '✗'}")
            print(f"  Battery Service: {'✓' if battery_found else '✗'}")
            
            # Step 3: Read sensor data
            sensor_data = await self.read_environmental_data()
            results['sensor_data'] = sensor_data
            results['tests']['data_reading'] = len(sensor_data) > 0
            
            # Validate sensor data ranges
            data_valid = True
            if 'temperature' in sensor_data:
                temp = sensor_data['temperature']
                if not (-40 <= temp <= 85):  # Sensor operating range
                    data_valid = False
            
            if 'humidity' in sensor_data:
                humidity = sensor_data['humidity']
                if not (0 <= humidity <= 100):
                    data_valid = False
            
            results['tests']['data_validation'] = data_valid
            
            # Step 4: Connection stability test
            stability = await self.test_connection_stability(duration=15)
            results['tests']['connection_stability'] = stability
            
            # Overall success
            all_critical_tests = [
                results['tests']['connection'],
                results['tests']['service_discovery'],
                results['tests']['data_reading']
            ]
            results['success'] = all(all_critical_tests)
            
        except Exception as e:
            print(f"✗ Test suite error: {e}")
            results['error'] = str(e)
        
        finally:
            await self.disconnect()
        
        return results
    
    def print_test_report(self, results: Dict[str, Any]):
        """Print formatted test report"""
        print("\n" + "="*60)
        print("BLE Test Report")
        print("="*60)
        print(f"Timestamp: {results['timestamp']}")
        print(f"Overall Success: {'✓ PASS' if results['success'] else '✗ FAIL'}")
        
        if 'tests' in results:
            print("\nTest Results:")
            for test_name, passed in results['tests'].items():
                status = "✓ PASS" if passed else "✗ FAIL"
                print(f"  {status}: {test_name}")
        
        if 'sensor_data' in results:
            print("\nSensor Data:")
            for sensor, value in results['sensor_data'].items():
                if sensor == 'temperature':
                    print(f"  Temperature: {value:.2f}°C")
                elif sensor == 'humidity':
                    print(f"  Humidity: {value:.1f}%")
                elif sensor == 'pressure':
                    print(f"  Pressure: {value:.1f}hPa")
                elif sensor == 'battery':
                    print(f"  Battery: {value}%")
        
        if 'services' in results:
            print(f"\nServices Found: {len(results['services'])}")
            for uuid, service in results['services'].items():
                print(f"  {service['name']} ({len(service['characteristics'])} characteristics)")

async def main():
    """Main entry point for BLE testing"""
    parser = argparse.ArgumentParser(description="Thingy:52 BLE Test Automation")
    parser.add_argument("--address", help="Specific BLE device address to test")
    parser.add_argument("--scan-only", action="store_true", help="Only scan for devices")
    parser.add_argument("--scan-time", type=int, default=10, help="Scan duration in seconds")
    
    args = parser.parse_args()
    
    tester = Thingy52BLETester()
    
    if args.scan_only:
        devices = await tester.scan_for_devices(args.scan_time)
        print(f"\nFound {len(devices)} potential Thingy:52 devices")
        for address, name in devices:
            print(f"  {address} - {name}")
        return
    
    # Run complete test suite
    results = await tester.run_complete_ble_test(args.address)
    tester.print_test_report(results)
    
    # Save results to file
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    import json
    with open(f"ble_test_results_{timestamp}.json", 'w') as f:
        json.dump(results, f, indent=2)
    
    print(f"\n✓ Results saved to ble_test_results_{timestamp}.json")
    
    sys.exit(0 if results['success'] else 1)

if __name__ == "__main__":
    asyncio.run(main())