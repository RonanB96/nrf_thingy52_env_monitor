from __future__ import annotations

import re
from typing import TYPE_CHECKING

import pytest

from .ble_gatt_client import DEVICE_BOOT_TIMEOUT_SECONDS, read_snapshot_sync

if TYPE_CHECKING:
    from twister_harness import DeviceAdapter


BLE_ADDRESS_RE = re.compile(r"Static BLE address set: ([0-9A-F:]{17})")
DEVICE_NAME_RE = re.compile(r"Device name: (T52-[A-Z0-9]+)")


def _extract_boot_identity(lines: list[str]) -> tuple[str, str]:
	address = ""
	name = ""

	for line in lines:
		address_match = BLE_ADDRESS_RE.search(line)
		if address_match:
			address = address_match.group(1)

		name_match = DEVICE_NAME_RE.search(line)
		if name_match:
			name = name_match.group(1)

	return address, name

def test_ble_gatt_sensor_reads(dut: "DeviceAdapter"):
	pytest.importorskip("bleak")
	pytest.importorskip("bluetooth_sig")

	boot_lines = dut.readlines_until(
		regex=r"BLE advertising started",
		timeout=DEVICE_BOOT_TIMEOUT_SECONDS,
		print_output=True,
	)
	address, name = _extract_boot_identity(boot_lines)

	snapshot = read_snapshot_sync(address=address, name=name)

	assert -40.0 <= snapshot.temperature_c <= 85.0
	assert 0.0 <= snapshot.humidity_percent <= 100.0
	assert 26_000.0 <= snapshot.pressure_pa <= 126_000.0
	assert 0 <= snapshot.battery_percent <= 100
	assert snapshot.uptime_seconds >= 0

	co2_unknown = snapshot.co2_ppm == 0xFFFF
	tvoc_unknown = snapshot.tvoc_ppb == 0xFFFF

	assert co2_unknown or 400 <= float(snapshot.co2_ppm) <= 5000
	assert tvoc_unknown or 0 <= snapshot.tvoc_ppb <= 65534