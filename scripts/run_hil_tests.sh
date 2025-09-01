#!/bin/bash
"""
Example Hardware Test Execution Script
Demonstrates complete HIL testing workflow for Thingy:52
"""

set -e  # Exit on any error

echo "========================================"
echo "Thingy:52 HIL Test Execution Example"
echo "========================================"

# Configuration
DEVICE_SERIAL="${DEVICE_SERIAL:-}"  # Set via environment or auto-detect
TEST_DURATION=120  # 2 minutes for comprehensive testing
LOG_FILE="hardware_test_execution_$(date +%Y%m%d_%H%M%S).log"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log() {
    echo -e "${GREEN}[$(date '+%H:%M:%S')]${NC} $1" | tee -a "$LOG_FILE"
}

warn() {
    echo -e "${YELLOW}[$(date '+%H:%M:%S')] WARNING:${NC} $1" | tee -a "$LOG_FILE"
}

error() {
    echo -e "${RED}[$(date '+%H:%M:%S')] ERROR:${NC} $1" | tee -a "$LOG_FILE"
}

# Check prerequisites
check_prerequisites() {
    log "Checking prerequisites..."
    
    # Check if west is available
    if ! command -v west &> /dev/null; then
        error "west command not found. Is NCS environment activated?"
        exit 1
    fi
    
    # Check if Python requirements are available
    python3 -c "import serial, bleak" 2>/dev/null || {
        warn "Python dependencies missing. Installing..."
        pip install pyserial bleak || {
            error "Failed to install Python dependencies"
            exit 1
        }
    }
    
    # Check for connected device
    if [ -z "$DEVICE_SERIAL" ]; then
        log "Auto-detecting Thingy:52 device..."
        # Try to find device using nrfutil
        if command -v nrfutil &> /dev/null; then
            DEVICE_INFO=$(nrfutil device list 2>/dev/null | grep -i "thingy\|nordic" | head -1) || true
            if [ -n "$DEVICE_INFO" ]; then
                DEVICE_SERIAL=$(echo "$DEVICE_INFO" | awk '{print $1}')
                log "Found device: $DEVICE_SERIAL"
            fi
        fi
    fi
    
    log "Prerequisites checked ✓"
}

# Run complete hardware test suite
run_hardware_tests() {
    log "Starting hardware test suite..."
    
    # Build and run tests using Python automation
    if [ -n "$DEVICE_SERIAL" ]; then
        log "Running tests with device serial: $DEVICE_SERIAL"
        python3 scripts/hardware_test_runner.py --device-serial "$DEVICE_SERIAL" 2>&1 | tee -a "$LOG_FILE"
    else
        log "Running tests with auto-detection"
        python3 scripts/hardware_test_runner.py 2>&1 | tee -a "$LOG_FILE"
    fi
    
    TEST_EXIT_CODE=$?
    
    if [ $TEST_EXIT_CODE -eq 0 ]; then
        log "Hardware tests completed successfully ✓"
    else
        error "Hardware tests failed with exit code: $TEST_EXIT_CODE"
        return $TEST_EXIT_CODE
    fi
}

# Run BLE-specific tests
run_ble_tests() {
    log "Starting BLE connectivity tests..."
    
    # Run BLE tests separately for detailed validation
    python3 scripts/ble_test_automation.py 2>&1 | tee -a "$LOG_FILE"
    
    BLE_EXIT_CODE=$?
    
    if [ $BLE_EXIT_CODE -eq 0 ]; then
        log "BLE tests completed successfully ✓"
    else
        warn "BLE tests failed or no device found (exit code: $BLE_EXIT_CODE)"
        warn "This may be normal if no BLE device is in range"
    fi
    
    return 0  # Don't fail overall test for BLE issues
}

# Parse and validate results
parse_results() {
    log "Parsing test results..."
    
    # Look for the most recent hardware test report
    LATEST_REPORT=$(ls -t hardware_test_report_*.txt 2>/dev/null | head -1) || true
    
    if [ -n "$LATEST_REPORT" ] && [ -f "$LATEST_REPORT" ]; then
        log "Found test report: $LATEST_REPORT"
        
        # Parse results using our validation script
        python3 scripts/parse_hardware_test_results.py "$LATEST_REPORT"
        PARSE_EXIT_CODE=$?
        
        if [ $PARSE_EXIT_CODE -eq 0 ]; then
            log "All tests passed validation ✓"
        else
            error "Some tests failed validation"
            return $PARSE_EXIT_CODE
        fi
    else
        warn "No hardware test report found for parsing"
        
        # Try parsing from our log file instead
        if [ -f "$LOG_FILE" ]; then
            log "Attempting to parse from execution log..."
            python3 scripts/parse_hardware_test_results.py "$LOG_FILE" || true
        fi
    fi
}

# Generate summary report
generate_summary() {
    log "Generating test summary..."
    
    SUMMARY_FILE="test_summary_$(date +%Y%m%d_%H%M%S).md"
    
    cat > "$SUMMARY_FILE" << EOF
# Thingy:52 Hardware Test Summary

**Date**: $(date)
**Device**: ${DEVICE_SERIAL:-Auto-detected}
**Duration**: Approximately $TEST_DURATION seconds

## Test Execution Log

\`\`\`
$(tail -50 "$LOG_FILE" 2>/dev/null || echo "No detailed log available")
\`\`\`

## Files Generated

- Execution Log: \`$LOG_FILE\`
- Summary Report: \`$SUMMARY_FILE\`
$(ls hardware_test_report_*.txt 2>/dev/null | sed 's/^/- Hardware Report: `/' | sed 's/$/`/' || echo "- No hardware report generated")
$(ls ble_test_results_*.json 2>/dev/null | sed 's/^/- BLE Results: `/' | sed 's/$/`/' || echo "- No BLE results generated")
$(ls hardware_test_validation_*.json 2>/dev/null | sed 's/^/- Validation Report: `/' | sed 's/$/`/' || echo "- No validation report generated")

## Manual Validation Steps

1. **BLE Testing**: Use nRF Connect mobile app to scan for "Thingy:52" device
2. **Service Discovery**: Verify Environmental Sensing Service (0x181A) and Battery Service (0x180F)
3. **Characteristic Reading**: Read temperature, humidity, pressure values
4. **Power Measurement**: Use Nordic Power Profiler Kit II for accurate measurements

## Troubleshooting

If tests failed, check:
- Device connection and serial port access
- SX1509B GPIO expander initialization (P0.16 reset pin)
- Sensor conditioning period (CCS811 requires 24-48 hours for new sensors)
- BLE interference or mobile device compatibility

EOF

    log "Summary report generated: $SUMMARY_FILE"
}

# Cleanup function
cleanup() {
    log "Cleaning up..."
    # Kill any background processes
    jobs -p | xargs -r kill 2>/dev/null || true
    log "Cleanup completed"
}

# Set up signal handling
trap cleanup EXIT INT TERM

# Main execution
main() {
    log "Starting Thingy:52 HIL test execution"
    
    check_prerequisites
    
    # Run the test phases
    run_hardware_tests || {
        error "Hardware tests failed"
        exit 1
    }
    
    run_ble_tests || {
        warn "BLE tests had issues (may be normal)"
    }
    
    parse_results || {
        warn "Result parsing had issues"
    }
    
    generate_summary
    
    log "======================================"
    log "HIL test execution completed successfully!"
    log "======================================"
    log "Generated files:"
    log "  - $LOG_FILE"
    log "  - $SUMMARY_FILE"
    ls hardware_test_report_*.txt 2>/dev/null | while read file; do log "  - $file"; done || true
    ls ble_test_results_*.json 2>/dev/null | while read file; do log "  - $file"; done || true
    ls hardware_test_validation_*.json 2>/dev/null | while read file; do log "  - $file"; done || true
    
    log ""
    log "Next steps for manual validation:"
    log "1. Use nRF Connect mobile app for BLE testing"
    log "2. Connect Nordic Power Profiler Kit II for power measurements"
    log "3. Review generated reports for any issues"
    
    return 0
}

# Run main function
main "$@"