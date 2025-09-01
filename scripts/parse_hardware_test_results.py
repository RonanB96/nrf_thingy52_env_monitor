#!/usr/bin/env python3
"""
Test Result Parser and Validator for Thingy:52 HIL Tests

This utility parses hardware test results from serial logs and validates
them against expected criteria, generating structured reports.
"""

import re
import json
import argparse
from pathlib import Path
from typing import Dict, List, Any, Optional
from datetime import datetime

class HardwareTestResultParser:
    """Parse and validate hardware test results"""
    
    def __init__(self):
        self.test_results: Dict[str, Dict] = {}
        self.test_metrics: Dict[str, float] = {}
        self.test_log_lines: List[str] = []
        
        # Test result patterns
        self.pass_pattern = re.compile(r"✓ PASS: (.+?)(?:\s+\((.+?)\))?$")
        self.fail_pattern = re.compile(r"✗ FAIL: (.+?)(?:\s+-\s+(.+?))?$")
        self.value_pattern = re.compile(r"(.+?):\s*([\d.-]+)\s*(\w+)?")
        
        # Expected ranges for validation
        self.validation_ranges = {
            'temperature': (-40.0, 85.0, '°C'),
            'humidity': (0.0, 100.0, '%'),
            'pressure': (800.0, 1200.0, 'hPa'),
            'battery': (0, 100, '%'),
            'eco2': (400, 8192, 'ppm'),
            'tvoc': (0, 1187, 'ppb'),
            'power_baseline': (0, 100, 'µA'),
            'power_sleep': (0, 50, 'µA'),
            'power_advertising': (0, 500, 'µA')
        }
    
    def parse_log_file(self, log_path: Path) -> Dict[str, Any]:
        """Parse hardware test log file"""
        if not log_path.exists():
            raise FileNotFoundError(f"Log file not found: {log_path}")
        
        with open(log_path, 'r') as f:
            lines = f.readlines()
        
        return self.parse_log_lines(lines)
    
    def parse_log_lines(self, lines: List[str]) -> Dict[str, Any]:
        """Parse list of log lines"""
        self.test_log_lines = [line.strip() for line in lines]
        
        for line in self.test_log_lines:
            self._parse_test_result_line(line)
            self._parse_metric_line(line)
        
        return self._generate_summary()
    
    def _parse_test_result_line(self, line: str):
        """Parse individual test result line"""
        # Check for pass result
        pass_match = self.pass_pattern.search(line)
        if pass_match:
            test_name = pass_match.group(1).strip()
            value_info = pass_match.group(2)
            
            self.test_results[test_name] = {
                'status': 'PASS',
                'value': value_info,
                'line': line
            }
            return
        
        # Check for fail result
        fail_match = self.fail_pattern.search(line)
        if fail_match:
            test_name = fail_match.group(1).strip()
            error_msg = fail_match.group(2)
            
            self.test_results[test_name] = {
                'status': 'FAIL',
                'error': error_msg,
                'line': line
            }
            return
    
    def _parse_metric_line(self, line: str):
        """Parse measurement value lines"""
        # Look for sensor readings
        if "temperature:" in line.lower():
            temp_match = re.search(r"temperature:\s*([\d.-]+)", line.lower())
            if temp_match:
                self.test_metrics['temperature'] = float(temp_match.group(1))
        
        if "humidity:" in line.lower():
            humid_match = re.search(r"humidity:\s*([\d.-]+)", line.lower())
            if humid_match:
                self.test_metrics['humidity'] = float(humid_match.group(1))
        
        if "pressure:" in line.lower():
            press_match = re.search(r"pressure:\s*([\d.-]+)", line.lower())
            if press_match:
                self.test_metrics['pressure'] = float(press_match.group(1))
        
        if "battery:" in line.lower():
            batt_match = re.search(r"battery.*?:\s*(\d+)", line.lower())
            if batt_match:
                self.test_metrics['battery'] = float(batt_match.group(1))
        
        # Power consumption measurements
        if "power" in line.lower() and "µa" in line.lower():
            power_match = re.search(r"(\d+)µa", line.lower())
            if power_match:
                if "baseline" in line.lower():
                    self.test_metrics['power_baseline'] = float(power_match.group(1))
                elif "sleep" in line.lower():
                    self.test_metrics['power_sleep'] = float(power_match.group(1))
                elif "advertising" in line.lower():
                    self.test_metrics['power_advertising'] = float(power_match.group(1))
    
    def _generate_summary(self) -> Dict[str, Any]:
        """Generate test summary with validation"""
        summary = {
            'timestamp': datetime.now().isoformat(),
            'total_tests': len(self.test_results),
            'passed_tests': len([r for r in self.test_results.values() if r['status'] == 'PASS']),
            'failed_tests': len([r for r in self.test_results.values() if r['status'] == 'FAIL']),
            'test_results': self.test_results,
            'measurements': self.test_metrics,
            'validation': self._validate_measurements(),
            'recommendations': self._generate_recommendations()
        }
        
        summary['success_rate'] = (summary['passed_tests'] / summary['total_tests'] * 100) if summary['total_tests'] > 0 else 0
        
        return summary
    
    def _validate_measurements(self) -> Dict[str, Any]:
        """Validate measurements against expected ranges"""
        validation = {}
        
        for metric, value in self.test_metrics.items():
            if metric in self.validation_ranges:
                min_val, max_val, unit = self.validation_ranges[metric]
                
                validation[metric] = {
                    'value': value,
                    'unit': unit,
                    'range': f"{min_val}-{max_val} {unit}",
                    'valid': min_val <= value <= max_val,
                    'status': 'PASS' if min_val <= value <= max_val else 'FAIL'
                }
                
                if not validation[metric]['valid']:
                    if value < min_val:
                        validation[metric]['issue'] = f"Below minimum ({min_val} {unit})"
                    else:
                        validation[metric]['issue'] = f"Above maximum ({max_val} {unit})"
        
        return validation
    
    def _generate_recommendations(self) -> List[str]:
        """Generate recommendations based on test results"""
        recommendations = []
        
        # Check for common failure patterns
        if any('SX1509B' in test and result['status'] == 'FAIL' 
               for test, result in self.test_results.items()):
            recommendations.append(
                "SX1509B GPIO expander failure detected. Check P0.16 reset pin state and device tree initialization order."
            )
        
        if any('CCS811' in test and result['status'] == 'FAIL' 
               for test, result in self.test_results.items()):
            recommendations.append(
                "CCS811 sensor issues detected. Verify SX1509B pins 10-12 control and conditioning period."
            )
        
        if any('BLE' in test and result['status'] == 'FAIL' 
               for test, result in self.test_results.items()):
            recommendations.append(
                "BLE functionality issues. Check Bluetooth configuration and antenna connections."
            )
        
        # Check measurement ranges
        validation = self._validate_measurements()
        for metric, val_info in validation.items():
            if not val_info['valid']:
                recommendations.append(
                    f"{metric.title()} measurement out of range: {val_info['value']} {val_info['unit']} "
                    f"(expected: {val_info['range']}). {val_info.get('issue', '')}"
                )
        
        # Power consumption recommendations
        if 'power_baseline' in self.test_metrics:
            power = self.test_metrics['power_baseline']
            if power > 100:
                recommendations.append(
                    f"High baseline power consumption ({power}µA). Check for sensors not properly powered down."
                )
        
        return recommendations
    
    def print_report(self, summary: Dict[str, Any]):
        """Print formatted test report"""
        print("="*70)
        print("HARDWARE TEST VALIDATION REPORT")
        print("="*70)
        print(f"Timestamp: {summary['timestamp']}")
        print(f"Total Tests: {summary['total_tests']}")
        print(f"Passed: {summary['passed_tests']}")
        print(f"Failed: {summary['failed_tests']}")
        print(f"Success Rate: {summary['success_rate']:.1f}%")
        
        print("\nTEST RESULTS:")
        print("-" * 50)
        for test_name, result in summary['test_results'].items():
            status_icon = "✓" if result['status'] == 'PASS' else "✗"
            print(f"{status_icon} {result['status']}: {test_name}")
            if result['status'] == 'FAIL' and 'error' in result:
                print(f"   Error: {result['error']}")
        
        if summary['measurements']:
            print("\nMEASUREMENTS:")
            print("-" * 50)
            for metric, value in summary['measurements'].items():
                print(f"{metric.replace('_', ' ').title()}: {value}")
        
        if summary['validation']:
            print("\nVALIDATION:")
            print("-" * 50)
            for metric, val_info in summary['validation'].items():
                status_icon = "✓" if val_info['valid'] else "✗"
                print(f"{status_icon} {metric.replace('_', ' ').title()}: "
                      f"{val_info['value']} {val_info['unit']} "
                      f"(range: {val_info['range']})")
                if not val_info['valid']:
                    print(f"   Issue: {val_info.get('issue', 'Out of range')}")
        
        if summary['recommendations']:
            print("\nRECOMMENDATIONS:")
            print("-" * 50)
            for i, rec in enumerate(summary['recommendations'], 1):
                print(f"{i}. {rec}")
        
        print("="*70)
    
    def save_json_report(self, summary: Dict[str, Any], output_path: Path):
        """Save report as JSON file"""
        with open(output_path, 'w') as f:
            json.dump(summary, f, indent=2)
        print(f"Report saved to: {output_path}")

def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description="Parse and validate Thingy:52 hardware test results")
    parser.add_argument("log_file", help="Path to hardware test log file")
    parser.add_argument("--output", "-o", help="Output JSON report file")
    parser.add_argument("--quiet", "-q", action="store_true", help="Suppress console output")
    
    args = parser.parse_args()
    
    log_path = Path(args.log_file)
    
    parser_obj = HardwareTestResultParser()
    
    try:
        summary = parser_obj.parse_log_file(log_path)
        
        if not args.quiet:
            parser_obj.print_report(summary)
        
        if args.output:
            output_path = Path(args.output)
        else:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            output_path = Path(f"hardware_test_validation_{timestamp}.json")
        
        parser_obj.save_json_report(summary, output_path)
        
        # Exit with error code if tests failed
        if summary['failed_tests'] > 0:
            print(f"\n⚠ {summary['failed_tests']} test(s) failed")
            return 1
        else:
            print(f"\n✓ All {summary['passed_tests']} tests passed")
            return 0
            
    except Exception as e:
        print(f"Error parsing test results: {e}")
        return 1

if __name__ == "__main__":
    exit(main())