"""Offline parser tests for GATT-path shell output (no hardware)."""

from __future__ import annotations

from pathlib import Path
import sys

import pytest

_ROOT = Path(__file__).resolve().parent
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

from shell_parse import (
    has_unknown_characteristic_error,
    parse_battery,
    parse_ccc,
    parse_ccs811_status,
    parse_connect_count,
    parse_decoded_humidity_pct,
    parse_decoded_pressure_hpa,
    parse_decoded_temp_c,
    parse_disconnect_count,
    parse_ess_status,
    parse_sm_status,
    parse_uptime_seconds,
    parse_wire_reads,
)

SM_IDLE = "armed=yes count=0\nvalid_mask=0x1f ts=1234\nT=21.50 C H=40.00 % P=101.325 kPa CO2=400 ppm TVOC=0 ppb bat=88% chg=no\n"
CONNECT = "simulated GATT connect (count=1)\n"
DISCONNECT = "simulated GATT disconnect (count=0)\n"
POLL = """--- poll 1/1 ---
temp wire [2]: b6 07
  decoded: 1974 (19.74 C)
humidity wire [2]: 88 13
  decoded: 5000 (50.00 %)
pressure wire [4]: 92 65 0c 00
  decoded: 812434 (812.43 hPa)
battery cache: 88% charging=no
co2 wire [2]: ff ff
  decoded: 0xFFFF (unknown)
tvoc wire [2]: ff ff
  decoded: 0xFFFF (unknown)
"""
CCS811 = "ready=no enabled=yes idle=no ble_connected=yes await_1s=no mode=2 cond_ms=1190000\n"
ESS = """temp known=yes notify=yes value=1974
humidity known=yes notify=no value=5000
pressure known=yes notify=no value=1013250
co2 known=yes notify=no value=65535
tvoc known=yes notify=no value=65535
"""
CCC_ON = "temp CCC notify on\n"
UNKNOWN = "unknown characteristic 'bogon'\n"


def test_parse_sm_status_idle() -> None:
    status = parse_sm_status(SM_IDLE)
    assert status.armed is True
    assert status.count == 0


def test_parse_connect_disconnect_counts() -> None:
    assert parse_connect_count(CONNECT) == 1
    assert parse_disconnect_count(DISCONNECT) == 0


def test_parse_poll_wires_and_battery() -> None:
    reads = parse_wire_reads(POLL)
    assert [read.name for read in reads] == ["temp", "humidity", "pressure", "co2", "tvoc"]
    assert reads[0].nbytes == 2
    assert reads[0].hex_bytes == bytes.fromhex("b6 07")
    level, charging = parse_battery(POLL)
    assert level == 88
    assert charging is False
    assert parse_decoded_temp_c(POLL) == pytest.approx(19.74)
    assert parse_decoded_humidity_pct(POLL) == pytest.approx(50.00)
    assert parse_decoded_pressure_hpa(POLL) == pytest.approx(812.43)
    assert parse_uptime_seconds("uptime wire [8]: 01 00 00 00 00 00 00 00\n  decoded: 1 s\n") == 1


def test_parse_ccs811_and_ess_and_ccc() -> None:
    ccs811 = parse_ccs811_status(CCS811)
    assert ccs811.ble_connected is True
    assert ccs811.mode == 2
    ess = parse_ess_status(ESS)
    assert ess["temp"].notify is True
    assert ess["co2"].value == 65535
    char, enabled = parse_ccc(CCC_ON)
    assert char == "temp"
    assert enabled is True


def test_unknown_characteristic_error() -> None:
    assert has_unknown_characteristic_error(UNKNOWN)
    assert not has_unknown_characteristic_error(CONNECT)


def test_parse_sm_status_missing_raises() -> None:
    with pytest.raises(ValueError):
        parse_sm_status("no status here")
