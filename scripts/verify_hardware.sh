#!/usr/bin/env bash
# verify_hardware.sh — confirms flash and serial access to the Thingy:52.
#
# Usage: ./scripts/verify_hardware.sh [build_dir]
#
# Exit 0: JLink detected, serial port accessible, firmware flashed, expected
#         boot log lines received.
# Exit 1: any check fails.
#
# The serial logger (scripts/serial_logger.py) is started first and kept open
# until this script explicitly kills it.  Flash runs in a separate process.
# This matches how an AI agent orchestrates hardware: one process owns the
# serial port, another owns the flash tool.

BUILD_DIR="${1:-app/build}"
# Can change
SERIAL_PORT="/dev/ttyUSB1"
BAUD=115200
LOG_FILE="$(mktemp /tmp/thingy52_boot_XXXXXX.log)"
BOOT_SETTLE_SECONDS=5

REQUIRED_PATTERNS=(
    "Starting BLE Environmental Monitor"
    "sensor_manager: Sensor manager initialized"
    "ble_advertiser: Legacy advertising started successfully"
)

cleanup() {
    if [[ -n "$LOGGER_PID" ]]; then
        kill "$LOGGER_PID" 2>/dev/null
        wait "$LOGGER_PID" 2>/dev/null
    fi
    rm -f "$LOG_FILE"
}
trap cleanup EXIT

echo "=== Thingy:52 Hardware Verification ==="

# 1. JLink detected
echo "[1/4] Checking JLink..."
if ! nrfutil device list 2>&1 | grep -q "jlink"; then
    echo "FAIL: No JLink device found"
    exit 1
fi
echo "  OK: JLink detected"

# Activate venv and environment before any Python or west usage.
source .venv/bin/activate
if ! source env.sh; then
    echo "FAIL: env.sh returned non-zero — check ZEPHYR_SDK_INSTALL_DIR"
    exit 1
fi

# 2. Serial port accessible
echo "[2/4] Checking serial port $SERIAL_PORT..."
if [[ ! -c "$SERIAL_PORT" ]]; then
    echo "FAIL: $SERIAL_PORT does not exist"
    exit 1
fi
if ! python3 -c "import serial; serial.Serial('$SERIAL_PORT', $BAUD).close()" 2>/dev/null; then
    echo "FAIL: cannot open $SERIAL_PORT (check dialout group membership and that pyserial is installed in .venv)"
    exit 1
fi
echo "  OK: $SERIAL_PORT accessible"

# 3. Start persistent logger BEFORE flash so post-reset boot output is
#    captured from the first byte.  The logger stays open until we kill it.
echo "[3/4] Starting serial logger then flashing..."
python3 scripts/serial_logger.py "$SERIAL_PORT" "$BAUD" "$LOG_FILE" &
LOGGER_PID=$!

sleep 0.5  # give the logger time to open the port before flash triggers reset
if ! west flash -d "${BUILD_DIR}/app" --runner jlink > /tmp/west_flash.log 2>&1; then
    echo "FAIL: west flash exited non-zero"
    cat /tmp/west_flash.log
    exit 1
fi
echo "  OK: west flash succeeded"

# Wait for device to finish booting, then explicitly stop the logger.
sleep "$BOOT_SETTLE_SECONDS"
kill "$LOGGER_PID" 2>/dev/null
wait "$LOGGER_PID" 2>/dev/null
LOGGER_PID=""

# 4. Verify expected boot patterns in the captured log.
echo "[4/4] Verifying boot log..."
FAIL=0
for pattern in "${REQUIRED_PATTERNS[@]}"; do
    if grep -qF "$pattern" "$LOG_FILE"; then
        echo "  OK: '$pattern'"
    else
        echo "  FAIL: missing '$pattern'"
        FAIL=1
    fi
done

if [[ $FAIL -eq 1 ]]; then
    echo ""
    echo "--- Captured log ---"
    cat "$LOG_FILE"
    exit 1
fi

echo ""
echo "=== PASS: Hardware verification complete ==="
exit 0
