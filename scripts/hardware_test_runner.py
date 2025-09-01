#!/usr/bin/env python3
"""
Hardware Test Runner for Thingy:52 Environmental Monitor

This script provides automated test execution and reporting for
Hardware-in-the-Loop (HIL) testing with actual Thingy:52 hardware.

Features:
- Automated build and flash of hardware test firmware
- Serial monitoring and log capture
- BLE connectivity testing with Python/Bleak
- Test result parsing and validation
- Report generation with pass/fail criteria

Requirements:
- Python 3.10+
- bleak (for BLE testing)
- pyserial (for serial monitoring)
- west (for building and flashing)
"""

import asyncio
import subprocess
import time
import re
import sys
import argparse
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Optional, Tuple

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("Error: pyserial not installed. Run: pip install pyserial")
    sys.exit(1)

try:
    from bleak import BleakScanner, BleakClient
except ImportError:
    print("Warning: bleak not installed. BLE testing will be skipped.")
    print("To enable BLE testing, run: pip install bleak")
    BleakScanner = None
    BleakClient = None

class Thingy52HardwareTester:
    """Main class for Thingy:52 hardware testing automation"""
    
    def __init__(self, device_serial: Optional[str] = None, build_dir: str = "build"):
        self.device_serial = device_serial
        self.build_dir = Path(build_dir)
        self.test_results: Dict[str, bool] = {}
        self.test_log: List[str] = []
        self.serial_port: Optional[str] = None
        self.thingy_device = None
        
    def find_thingy52_device(self) -> Optional[str]:
        """Find connected Thingy:52 device serial port"""
        print("Scanning for Thingy:52 device...")
        
        # Look for Nordic devices
        for port in serial.tools.list_ports.comports():
            if "Nordic" in str(port.manufacturer) or "Thingy" in str(port.description):
                print(f"Found potential Thingy:52 device: {port.device}")
                return port.device
                
        # Fallback to common USB serial devices
        for port in serial.tools.list_ports.comports():
            if "USB" in str(port.description) or "ttyUSB" in str(port.device):
                print(f"Found USB serial device: {port.device}")
                return port.device
                
        return None
    
    def build_hardware_test_firmware(self) -> bool:
        """Build the hardware test firmware"""
        print("Building hardware test firmware...")
        
        try:
            # Change to hardware test directory
            test_dir = Path("app/tests/hardware")
            if not test_dir.exists():
                print(f"Error: Hardware test directory {test_dir} not found")
                return False
                
            # Build command
            cmd = ["west", "build", "-b", "thingy52", "-p", "always", str(test_dir)]
            
            print(f"Running: {' '.join(cmd)}")
            result = subprocess.run(cmd, capture_output=True, text=True, cwd=".")
            
            if result.returncode == 0:
                print("✓ Hardware test firmware built successfully")
                return True
            else:
                print(f"✗ Build failed: {result.stderr}")
                return False
                
        except subprocess.CalledProcessError as e:
            print(f"✗ Build error: {e}")
            return False
        except FileNotFoundError:
            print("✗ Error: 'west' command not found. Is NCS environment activated?")
            return False
    
    def flash_firmware(self) -> bool:
        """Flash the hardware test firmware to Thingy:52"""
        print("Flashing hardware test firmware...")
        
        try:
            cmd = ["west", "flash"]
            
            if self.device_serial:
                # Add device serial if specified
                cmd.extend(["--dev-id", self.device_serial])
            
            result = subprocess.run(cmd, capture_output=True, text=True)
            
            if result.returncode == 0:
                print("✓ Firmware flashed successfully")
                return True
            else:
                print(f"✗ Flash failed: {result.stderr}")
                return False
                
        except subprocess.CalledProcessError as e:
            print(f"✗ Flash error: {e}")
            return False
    
    def reset_device(self) -> bool:
        """Reset the Thingy:52 device"""
        print("Resetting device...")
        
        try:
            if self.device_serial:
                cmd = ["nrfutil", "device", "reset", "--serial-number", self.device_serial]
            else:
                cmd = ["nrfutil", "device", "reset"]
            
            result = subprocess.run(cmd, capture_output=True, text=True)
            
            if result.returncode == 0:
                print("✓ Device reset successfully")
                return True
            else:
                print(f"✗ Reset failed: {result.stderr}")
                return False
                
        except subprocess.CalledProcessError as e:
            print(f"✗ Reset error: {e}")
            return False
        except FileNotFoundError:
            print("✗ nrfutil not found - using manual reset")
            return True  # Continue without reset
    
    def monitor_serial_output(self, duration: int = 60) -> List[str]:
        """Monitor serial output for test results"""
        if not self.serial_port:
            self.serial_port = self.find_thingy52_device()
            
        if not self.serial_port:
            print("✗ No serial port found")
            return []
        
        print(f"Monitoring serial output on {self.serial_port} for {duration} seconds...")
        
        try:
            with serial.Serial(self.serial_port, 115200, timeout=1) as ser:
                start_time = time.time()
                output_lines = []
                
                while time.time() - start_time < duration:
                    try:
                        line = ser.readline().decode('utf-8', errors='ignore').strip()
                        if line:
                            print(f"SERIAL: {line}")
                            output_lines.append(line)
                            self.test_log.append(line)
                    except UnicodeDecodeError:
                        continue
                    except KeyboardInterrupt:
                        print("Serial monitoring interrupted")
                        break
                
                return output_lines
                
        except serial.SerialException as e:
            print(f"✗ Serial error: {e}")
            return []
    
    async def scan_for_thingy52_ble(self, scan_time: int = 10) -> Optional[str]:
        """Scan for Thingy:52 BLE device"""
        if not BleakScanner:
            print("⚠ BLE scanning not available (bleak not installed)")
            return None
            
        print(f"Scanning for Thingy:52 BLE device for {scan_time} seconds...")
        
        devices = await BleakScanner.discover(timeout=scan_time)
        
        for device in devices:
            name = device.name or ""
            if "Thingy" in name or "thingy" in name.lower():
                print(f"✓ Found Thingy:52 BLE device: {device.name} ({device.address})")
                return device.address
                
        print("⚠ No Thingy:52 BLE device found")
        return None
    
    async def test_ble_connectivity(self) -> bool:
        """Test BLE connectivity with Thingy:52"""
        if not BleakClient:
            print("⚠ BLE testing skipped (bleak not installed)")
            return False
            
        print("Testing BLE connectivity...")
        
        # Scan for device
        device_address = await self.scan_for_thingy52_ble()
        if not device_address:
            return False
        
        try:
            async with BleakClient(device_address) as client:
                print(f"✓ Connected to {device_address}")
                
                # Discover services
                services = await client.get_services()
                service_count = len(services.services)
                print(f"✓ Discovered {service_count} BLE services")
                
                # Look for expected services
                ess_found = False
                battery_found = False
                
                for service in services.services:
                    uuid = str(service.uuid).lower()
                    if "181a" in uuid:  # ESS service
                        ess_found = True
                        print("✓ Environmental Sensing Service found")
                    elif "180f" in uuid:  # Battery service
                        battery_found = True
                        print("✓ Battery Service found")
                
                self.test_results["BLE_Connection"] = True
                self.test_results["ESS_Service"] = ess_found
                self.test_results["Battery_Service"] = battery_found
                
                return True
                
        except Exception as e:
            print(f"✗ BLE connection failed: {e}")
            return False
    
    def parse_test_results(self, log_lines: List[str]) -> Dict[str, bool]:
        """Parse test results from serial log"""
        results = {}
        
        # Look for test result patterns
        pass_pattern = re.compile(r"✓ PASS: (.+)")
        fail_pattern = re.compile(r"✗ FAIL: (.+)")
        
        for line in log_lines:
            pass_match = pass_pattern.search(line)
            if pass_match:
                test_name = pass_match.group(1).split(" (")[0]  # Remove extra info
                results[test_name] = True
                
            fail_match = fail_pattern.search(line)
            if fail_match:
                test_name = fail_match.group(1).split(" -")[0]  # Remove error message
                results[test_name] = False
        
        return results
    
    def generate_report(self) -> str:
        """Generate test report"""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        
        report = f"""
Thingy:52 Hardware-in-the-Loop Test Report
==========================================
Date: {timestamp}
Device: {self.device_serial or 'Auto-detected'}

Test Results Summary:
"""
        
        if self.test_results:
            passed = sum(1 for result in self.test_results.values() if result)
            total = len(self.test_results)
            success_rate = (passed / total * 100) if total > 0 else 0
            
            report += f"Total Tests: {total}\n"
            report += f"Passed: {passed}\n" 
            report += f"Failed: {total - passed}\n"
            report += f"Success Rate: {success_rate:.1f}%\n\n"
            
            report += "Detailed Results:\n"
            for test_name, passed in self.test_results.items():
                status = "PASS" if passed else "FAIL"
                report += f"  {status}: {test_name}\n"
        else:
            report += "No test results available\n"
        
        report += f"\nTest Log:\n"
        report += "\n".join(self.test_log[-50:])  # Last 50 lines
        
        return report
    
    async def run_complete_test_suite(self) -> bool:
        """Run the complete hardware test suite"""
        print("Starting Thingy:52 Hardware Test Suite")
        print("=" * 50)
        
        # Step 1: Build firmware
        if not self.build_hardware_test_firmware():
            return False
        
        # Step 2: Flash firmware
        if not self.flash_firmware():
            return False
        
        # Step 3: Reset device
        self.reset_device()
        time.sleep(2)
        
        # Step 4: Monitor serial output for hardware tests
        print("\nRunning hardware tests...")
        log_lines = self.monitor_serial_output(duration=120)  # 2 minutes for all tests
        
        # Step 5: Parse results
        self.test_results.update(self.parse_test_results(log_lines))
        
        # Step 6: BLE testing
        print("\nRunning BLE tests...")
        ble_success = await self.test_ble_connectivity()
        self.test_results["BLE_Overall"] = ble_success
        
        # Step 7: Generate report
        report = self.generate_report()
        
        # Save report to file
        report_file = f"hardware_test_report_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt"
        with open(report_file, 'w') as f:
            f.write(report)
        
        print(f"\n✓ Test report saved to: {report_file}")
        print("\nTest Summary:")
        print(report)
        
        return len(self.test_results) > 0

async def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description="Thingy:52 Hardware Test Runner")
    parser.add_argument("--device-serial", help="Device serial number for flashing")
    parser.add_argument("--build-only", action="store_true", help="Only build firmware")
    parser.add_argument("--monitor-only", action="store_true", help="Only monitor serial output")
    parser.add_argument("--ble-only", action="store_true", help="Only run BLE tests")
    
    args = parser.parse_args()
    
    tester = Thingy52HardwareTester(device_serial=args.device_serial)
    
    if args.build_only:
        success = tester.build_hardware_test_firmware()
    elif args.monitor_only:
        lines = tester.monitor_serial_output(duration=60)
        print(f"Captured {len(lines)} lines of output")
        success = len(lines) > 0
    elif args.ble_only:
        success = await tester.test_ble_connectivity()
    else:
        success = await tester.run_complete_test_suite()
    
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    asyncio.run(main())