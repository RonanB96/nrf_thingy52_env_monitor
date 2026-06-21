from __future__ import annotations

import argparse
import asyncio
import importlib.util
import json
import os
import re
import subprocess
import sys
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, TypeVar, cast

from bluetooth_sig import BluetoothSIGTranslator, Device
from bluetooth_sig.gatt.characteristics import (
    BatteryLevelCharacteristic,
    CO2ConcentrationCharacteristic,
    HumidityCharacteristic,
    PressureCharacteristic,
    TemperatureCharacteristic,
    VOCConcentrationCharacteristic,
)
from bluetooth_sig.gatt.characteristics.base import BaseCharacteristic
from bluetooth_sig.gatt.exceptions import SpecialValueDetectedError
from bluetooth_sig.gatt.services.battery_service import BatteryService
from bluetooth_sig.gatt.services.environmental_sensing import EnvironmentalSensingService

T = TypeVar("T")

DEFAULT_DEVICE_NAME_PREFIX = "T52-"
DEFAULT_SCAN_TIMEOUT_SECONDS = 8.0
DEFAULT_SCAN_ATTEMPTS = 6
DEVICE_BOOT_TIMEOUT_SECONDS = 120.0
BLUEZ_PATH_ADAPTER_RE = re.compile(r"/org/bluez/(hci\d+)/")


def _load_local_module(module_name: str, filename: str) -> Any:
    module_path = Path(__file__).with_name(filename)
    spec = importlib.util.spec_from_file_location(module_name, module_path)
    if spec is None or spec.loader is None:
        raise ImportError(f"Unable to load {filename} from {module_path}")

    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


_custom_gatt = _load_local_module("uptime_service", "uptime_service.py")
_bleak_manager_mod = _load_local_module("thingy52_bleak_connection_manager", "bleak_connection_manager.py")

Thingy52BleakClientManager = _bleak_manager_mod.Thingy52BleakClientManager
Thingy52UptimeCharacteristic = _custom_gatt.Thingy52UptimeCharacteristic
UPTIME_SERVICE_UUID = str(_custom_gatt.UPTIME_SERVICE_UUID).lower()

ESS_SERVICE_UUID = str(EnvironmentalSensingService().uuid).lower()
BATTERY_SERVICE_UUID = str(BatteryService().uuid).lower()


@dataclass(frozen=True)
class GattSnapshot:
    temperature_c: float
    humidity_percent: float
    pressure_pa: float
    co2_ppm: float | int
    tvoc_ppb: int
    battery_percent: int
    uptime_seconds: int
    special_values: dict[str, str] = field(default_factory=dict)


@dataclass(frozen=True)
class DiscoveredDevice:
    address: str
    name: str | None
    local_name: str | None
    rssi: int | None
    service_uuids: list[str]
    manufacturer_data: dict[int, bytes]
    service_data: dict[str, bytes]


def _translator() -> BluetoothSIGTranslator:
    return BluetoothSIGTranslator.get_instance()


def _ensure_custom_gatt_registered() -> None:
    _custom_gatt.register_thingy52_custom_gatt()


def _load_bleak():
    from bleak import BleakClient, BleakScanner
    from bleak.backends.device import BLEDevice

    return BleakClient, BleakScanner, BLEDevice


def _resolve_adapter(adapter: str = "") -> str:
    return adapter.strip() or os.getenv("THINGY52_BLE_ADAPTER", "").strip()


def _bluez_args(adapter: str) -> Any:
    return {"adapter": adapter} if adapter else {}


def _adapter_from_device(device: Any) -> str:
    details = getattr(device, "details", None)
    if not isinstance(details, dict):
        return ""

    device_path = cast(Any, details).get("path")
    if not isinstance(device_path, str):
        return ""

    match = BLUEZ_PATH_ADAPTER_RE.search(device_path)
    return match.group(1) if match else ""


def _cached_bluez_device(*, address: str, name: str = "", adapter: str = ""):
    _, _, BLEDevice = _load_bleak()

    if not address:
        return None

    target_suffix = f"dev_{address.upper().replace(':', '_')}"
    adapter_prefix = f"/org/bluez/{adapter}/" if adapter else "/org/bluez/"

    try:
        result = subprocess.run(
            ["busctl", "tree", "org.bluez"],
            capture_output=True,
            text=True,
            check=True,
        )
    except (FileNotFoundError, subprocess.CalledProcessError):
        return None

    for line in result.stdout.splitlines():
        candidate = line.strip().lstrip("├─").lstrip("└─").strip()
        if not candidate.startswith(adapter_prefix):
            continue
        if target_suffix not in candidate:
            continue
        return BLEDevice(address, name or address, {"path": candidate})

    return None


def _ble_device_for_connect(device: Any) -> Any:
    """Return a Bleak device handle without a stale BlueZ object path."""
    _, _, BLEDevice = _load_bleak()
    address = getattr(device, "address", "")
    name = getattr(device, "name", "") or address
    return BLEDevice(address, name, {})


def _prefer_cached_bluez_device(device: Any, *, adapter: str = "") -> Any:
    cached_device = _cached_bluez_device(
        address=getattr(device, "address", ""),
        name=getattr(device, "name", "") or "",
        adapter=adapter,
    )
    if cached_device is not None:
        return cached_device
    return _ble_device_for_connect(device)


def _print_special_value(exc: SpecialValueDetectedError) -> None:
    raw_text = f"0x{exc.raw_int:X}" if exc.raw_int is not None else "n/a"
    print(
        f"{exc.name}: special value — {exc.special_value.meaning} (raw={raw_text})",
        file=sys.stderr,
        flush=True,
    )


def decode_characteristic(
    characteristic: type[BaseCharacteristic[T]],
    raw_data: bytes | bytearray,
) -> tuple[T | int, str | None]:
    """Decode bytes through a registered characteristic class."""
    try:
        return characteristic().parse_value(raw_data), None
    except SpecialValueDetectedError as exc:
        _print_special_value(exc)
        if exc.raw_int is None:
            raise
        return exc.raw_int, exc.special_value.meaning


async def _read_characteristic(
    device: Device,
    characteristic: type[BaseCharacteristic[T]],
) -> tuple[T | int, str | None]:
    """Read and decode a GATT characteristic via bluetooth_sig.Device."""
    try:
        value = await device.read(characteristic)
        if value is None:
            raise RuntimeError(f"No value returned for {characteristic.__name__}")
        return value, None
    except SpecialValueDetectedError as exc:
        _print_special_value(exc)
        if exc.raw_int is None:
            raise
        return exc.raw_int, exc.special_value.meaning


def decode_reference_payloads() -> dict[str, float | int | dict[str, str]]:
    """Offline decode check using registered characteristic classes (no BLE hardware)."""
    _ensure_custom_gatt_registered()

    temperature, _ = decode_characteristic(TemperatureCharacteristic, bytearray([0xF6, 0x09]))
    humidity, _ = decode_characteristic(HumidityCharacteristic, bytearray([0x96, 0x19]))
    pressure, _ = decode_characteristic(PressureCharacteristic, bytearray([0x0A, 0x80, 0x0F, 0x00]))
    co2, co2_special = decode_characteristic(CO2ConcentrationCharacteristic, bytearray([0x90, 0x01]))
    tvoc, _ = decode_characteristic(VOCConcentrationCharacteristic, bytearray([0xE8, 0x03]))
    battery, _ = decode_characteristic(BatteryLevelCharacteristic, bytearray([0x55]))
    uptime, _ = decode_characteristic(Thingy52UptimeCharacteristic, bytearray([0x10, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]))

    special_values: dict[str, str] = {}
    if co2_special:
        special_values["co2_ppm"] = co2_special

    return {
        "temperature_c": float(temperature),
        "humidity_percent": float(humidity),
        "pressure_pa": float(pressure),
        "co2_ppm": co2,
        "tvoc_ppb": int(tvoc),
        "battery_percent": int(battery),
        "uptime_seconds": int(uptime),
        "special_values": special_values,
    }


async def discover_target_device(
    *,
    address: str = "",
    name: str = "",
    name_prefix: str = DEFAULT_DEVICE_NAME_PREFIX,
    adapter: str = "",
    scan_timeout_seconds: float = DEFAULT_SCAN_TIMEOUT_SECONDS,
    scan_attempts: int = DEFAULT_SCAN_ATTEMPTS,
    verbose: bool = False,
):
    _, BleakScanner, _ = _load_bleak()

    target_address = address.strip().lower() or os.getenv("THINGY52_BLE_ADDRESS", "").strip().lower()
    target_name = name.strip() or os.getenv("THINGY52_BLE_NAME", "").strip()
    target_prefix = (
        name_prefix.strip()
        or os.getenv("THINGY52_BLE_NAME_PREFIX", DEFAULT_DEVICE_NAME_PREFIX).strip()
    )
    target_adapter = _resolve_adapter(adapter)
    cached_device = _cached_bluez_device(
        address=target_address,
        name=target_name,
        adapter=target_adapter,
    )
    if cached_device is not None:
        if verbose:
            print(
                f"using cached BlueZ device path on "
                f"adapter={_adapter_from_device(cached_device) or target_adapter or 'default'}",
                flush=True,
            )
        return cached_device

    for _ in range(scan_attempts):
        if verbose:
            print(
                f"scan attempt {_ + 1}/{scan_attempts} "
                f"timeout={scan_timeout_seconds}s "
                f"adapter={target_adapter or 'default'}",
                flush=True,
            )
        devices = await BleakScanner.discover(
            timeout=scan_timeout_seconds,
            bluez=_bluez_args(target_adapter),
        )
        for device in devices:
            device_address = getattr(device, "address", "").lower()
            device_name = getattr(device, "name", "") or ""

            if target_address and device_address == target_address:
                return _prefer_cached_bluez_device(device, adapter=target_adapter)

            if target_name and device_name == target_name:
                return _prefer_cached_bluez_device(device, adapter=target_adapter)

            if not target_address and not target_name and device_name.startswith(target_prefix):
                return _prefer_cached_bluez_device(device, adapter=target_adapter)

    cached_device = _cached_bluez_device(
        address=target_address,
        name=target_name,
        adapter=target_adapter,
    )
    if cached_device is not None:
        return cached_device

    raise RuntimeError(
        "BLE peripheral was not discovered. Set THINGY52_BLE_ADDRESS or "
        "THINGY52_BLE_NAME if multiple devices are advertising."
    )


async def list_devices(
    *,
    adapter: str = "",
    scan_timeout_seconds: float = DEFAULT_SCAN_TIMEOUT_SECONDS,
) -> list[DiscoveredDevice]:
    _, BleakScanner, _ = _load_bleak()
    target_adapter = _resolve_adapter(adapter)

    devices = cast(
        dict[str, tuple[Any, Any]],
        await BleakScanner.discover(
            timeout=scan_timeout_seconds,
            return_adv=True,
            bluez=_bluez_args(target_adapter),
        ),
    )
    results: list[DiscoveredDevice] = []

    for address, (device, adv) in devices.items():
        results.append(
            DiscoveredDevice(
                address=address,
                name=getattr(device, "name", None),
                local_name=adv.local_name,
                rssi=adv.rssi,
                service_uuids=list(adv.service_uuids or []),
                manufacturer_data=dict(adv.manufacturer_data or {}),
                service_data=dict(adv.service_data or {}),
            )
        )

    results.sort(key=lambda item: (item.rssi is None, -(item.rssi or -999)))
    return results


async def read_snapshot(
    *,
    address: str = "",
    name: str = "",
    name_prefix: str = DEFAULT_DEVICE_NAME_PREFIX,
    adapter: str = "",
    scan_timeout_seconds: float = DEFAULT_SCAN_TIMEOUT_SECONDS,
    scan_attempts: int = DEFAULT_SCAN_ATTEMPTS,
    verbose: bool = False,
) -> GattSnapshot:
    _ensure_custom_gatt_registered()

    target_adapter = _resolve_adapter(adapter)
    ble_device = await discover_target_device(
        address=address,
        name=name,
        name_prefix=name_prefix,
        adapter=target_adapter,
        scan_timeout_seconds=scan_timeout_seconds,
        scan_attempts=scan_attempts,
        verbose=verbose,
    )
    client_adapter = target_adapter or _adapter_from_device(ble_device)
    expected_services = {ESS_SERVICE_UUID, BATTERY_SERVICE_UUID, UPTIME_SERVICE_UUID}

    if verbose:
        print(
            f"connecting via adapter={client_adapter or 'default'} "
            f"device={getattr(ble_device, 'address', 'unknown')}",
            flush=True,
        )

    manager = Thingy52BleakClientManager(
        ble_device,
        adapter=client_adapter,
        services=list(expected_services),
    )
    sig_device = Device(manager, _translator())  # type: ignore[arg-type]

    special_values: dict[str, str] = {}

    try:
        await sig_device.connect()

        service_uuids = {service.uuid.lower() for service in manager.client.services}
        missing_services = expected_services - service_uuids
        if missing_services:
            raise RuntimeError(f"Missing expected services: {sorted(missing_services)}")

        temperature, _ = await _read_characteristic(sig_device, TemperatureCharacteristic)
        humidity, _ = await _read_characteristic(sig_device, HumidityCharacteristic)
        pressure, _ = await _read_characteristic(sig_device, PressureCharacteristic)
        co2, co2_special = await _read_characteristic(sig_device, CO2ConcentrationCharacteristic)
        tvoc, tvoc_special = await _read_characteristic(sig_device, VOCConcentrationCharacteristic)
        battery, _ = await _read_characteristic(sig_device, BatteryLevelCharacteristic)
        uptime, _ = await _read_characteristic(sig_device, Thingy52UptimeCharacteristic)

        if co2_special:
            special_values["co2_ppm"] = co2_special
        if tvoc_special:
            special_values["tvoc_ppb"] = tvoc_special
    finally:
        await sig_device.disconnect()

    return GattSnapshot(
        temperature_c=float(temperature),
        humidity_percent=float(humidity),
        pressure_pa=float(pressure),
        co2_ppm=co2,
        tvoc_ppb=int(tvoc),
        battery_percent=int(battery),
        uptime_seconds=int(uptime),
        special_values=special_values,
    )


def read_snapshot_sync(**kwargs: Any) -> GattSnapshot:
    return asyncio.run(read_snapshot(**kwargs))


def list_devices_sync(
    *,
    scan_timeout_seconds: float,
    adapter: str = "",
) -> list[DiscoveredDevice]:
    return asyncio.run(
        list_devices(
            scan_timeout_seconds=scan_timeout_seconds,
            adapter=adapter,
        )
    )


def _build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Read Thingy:52 BLE GATT sensors")
    parser.add_argument("--address", default="", help="BLE address to connect to")
    parser.add_argument("--name", default="", help="Exact BLE name to connect to")
    parser.add_argument(
        "--adapter",
        default="",
        help="BlueZ adapter to use, for example hci1",
    )
    parser.add_argument(
        "--name-prefix",
        default=DEFAULT_DEVICE_NAME_PREFIX,
        help="Fallback BLE name prefix used during discovery",
    )
    parser.add_argument(
        "--scan-timeout",
        type=float,
        default=DEFAULT_SCAN_TIMEOUT_SECONDS,
        help="Seconds to spend per discovery attempt",
    )
    parser.add_argument(
        "--scan-attempts",
        type=int,
        default=DEFAULT_SCAN_ATTEMPTS,
        help="Number of discovery attempts before failing",
    )
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="Decode known-good sample payloads without using BLE hardware",
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="List discovered BLE devices and advertisement metadata, then exit",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print scan progress while discovering the target device",
    )
    return parser


def main() -> int:
    args = _build_arg_parser().parse_args()

    if args.self_test:
        print(json.dumps(decode_reference_payloads(), indent=2, sort_keys=True))
        return 0

    if args.list:
        devices = list_devices_sync(
            scan_timeout_seconds=args.scan_timeout,
            adapter=args.adapter,
        )
        print(
            json.dumps(
                [
                    {
                        **asdict(device),
                        "manufacturer_data": {
                            str(key): value.hex()
                            for key, value in device.manufacturer_data.items()
                        },
                        "service_data": {
                            key: value.hex() for key, value in device.service_data.items()
                        },
                    }
                    for device in devices
                ],
                indent=2,
                sort_keys=True,
            )
        )
        return 0

    snapshot = read_snapshot_sync(
        address=args.address,
        name=args.name,
        adapter=args.adapter,
        name_prefix=args.name_prefix,
        scan_timeout_seconds=args.scan_timeout,
        scan_attempts=args.scan_attempts,
        verbose=args.verbose,
    )
    print(json.dumps(asdict(snapshot), indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
