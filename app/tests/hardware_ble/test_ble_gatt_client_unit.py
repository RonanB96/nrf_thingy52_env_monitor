from __future__ import annotations

import importlib.util
import sys
from io import StringIO
from pathlib import Path

import pytest

pytest.importorskip("bluetooth_sig")

from bluetooth_sig.gatt.characteristics import (
    CO2ConcentrationCharacteristic,
    TemperatureCharacteristic,
    VOCConcentrationCharacteristic,
)
from bluetooth_sig.gatt.exceptions import SpecialValueDetectedError
from bluetooth_sig.gatt.services.battery_service import BatteryService
from bluetooth_sig.gatt.services.environmental_sensing import EnvironmentalSensingService

_CLIENT_PATH = Path(__file__).with_name("pytest") / "ble_gatt_client.py"
_SPEC = importlib.util.spec_from_file_location("thingy52_ble_gatt_client", _CLIENT_PATH)
assert _SPEC and _SPEC.loader
_CLIENT = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = _CLIENT
_SPEC.loader.exec_module(_CLIENT)

_CUSTOM_GATT_PATH = Path(__file__).with_name("pytest") / "uptime_service.py"
_CUSTOM_SPEC = importlib.util.spec_from_file_location("uptime_service", _CUSTOM_GATT_PATH)
assert _CUSTOM_SPEC and _CUSTOM_SPEC.loader
_CUSTOM_GATT = importlib.util.module_from_spec(_CUSTOM_SPEC)
sys.modules[_CUSTOM_SPEC.name] = _CUSTOM_GATT
_CUSTOM_SPEC.loader.exec_module(_CUSTOM_GATT)


def test_decode_reference_payloads_matches_sig_samples() -> None:
    payloads = _CLIENT.decode_reference_payloads()

    assert payloads == {
        "temperature_c": 25.5,
        "humidity_percent": 65.5,
        "pressure_pa": 101581.8,
        "co2_ppm": 400,
        "tvoc_ppb": 1000,
        "battery_percent": 85,
        "uptime_seconds": 3600,
        "special_values": {},
    }


@pytest.mark.parametrize(
    "characteristic",
    [CO2ConcentrationCharacteristic, VOCConcentrationCharacteristic],
)
def test_decode_characteristic_reports_special_value_meaning(characteristic: type) -> None:
    stderr = StringIO()
    old_stderr = sys.stderr
    sys.stderr = stderr
    try:
        value, meaning = _CLIENT.decode_characteristic(characteristic, bytearray([0xFF, 0xFF]))
    finally:
        sys.stderr = old_stderr

    assert value == 0xFFFF
    assert meaning == "value is not known"
    assert "special value" in stderr.getvalue()
    assert "value is not known" in stderr.getvalue()


def test_uptime_custom_characteristic_round_trip() -> None:
    _CUSTOM_GATT.register_thingy52_custom_gatt()
    uptime = _CUSTOM_GATT.Thingy52UptimeCharacteristic()
    raw = uptime.build_value(3600)
    assert uptime.parse_value(raw) == 3600


def test_sig_service_uuids_match_registry() -> None:
    assert _CLIENT.ESS_SERVICE_UUID == str(EnvironmentalSensingService().uuid).lower()
    assert _CLIENT.BATTERY_SERVICE_UUID == str(BatteryService().uuid).lower()
    assert _CLIENT.UPTIME_SERVICE_UUID == str(_CUSTOM_GATT.UPTIME_SERVICE_UUID).lower()
    assert (
        str(TemperatureCharacteristic().uuid).lower()
        == "00002a6e-0000-1000-8000-00805f9b34fb"
    )


def test_special_value_is_not_a_parse_failure() -> None:
    with pytest.raises(SpecialValueDetectedError) as exc_info:
        CO2ConcentrationCharacteristic().parse_value(bytearray([0xFF, 0xFF]))

    assert exc_info.value.special_value.meaning == "value is not known"
