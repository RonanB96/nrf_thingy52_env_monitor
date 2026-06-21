"""Minimal Bleak adapter for bluetooth_sig.device.Device.

Implements ClientManagerProtocol so Thingy:52 test tooling can delegate
SIG characteristic decoding to the library instead of parsing payloads locally.
"""

from __future__ import annotations

import asyncio
import contextlib
from collections.abc import Callable
from typing import Any, cast

from bluetooth_sig.device.client import ClientManagerProtocol
from bluetooth_sig.types.advertising.result import AdvertisementData
from bluetooth_sig.types.device_types import DeviceService
from bluetooth_sig.types.uuid import BluetoothUUID


class Thingy52BleakClientManager(ClientManagerProtocol):
    """Bleak transport wrapper with BlueZ adapter and service-filter support."""

    supports_scanning = False

    def __init__(
        self,
        ble_device: Any,
        *,
        adapter: str = "",
        services: list[str] | None = None,
        timeout: float = 30.0,
    ) -> None:
        super().__init__(getattr(ble_device, "address", ble_device if isinstance(ble_device, str) else ""))
        self._ble_device = ble_device
        self._adapter = adapter.strip()
        self._services = services
        self._timeout = timeout
        self._client: Any = None
        self._scanner: Any = None
        self._disconnected_callback: Callable[[], None] | None = None

    @property
    def client(self) -> Any:
        if self._client is None:
            raise RuntimeError("BLE client is not connected")
        return self._client

    def _bluez_args(self) -> dict[str, str]:
        return {"adapter": self._adapter} if self._adapter else {}

    def _target_address(self) -> str:
        if isinstance(self._ble_device, str):
            return self._ble_device
        return str(getattr(self._ble_device, "address", ""))

    def _bleak_client_target(self) -> str | Any:
        details = getattr(self._ble_device, "details", None)
        if isinstance(details, dict) and details.get("path"):
            return self._ble_device
        address = self._target_address()
        if address:
            return address
        raise ValueError("BLE device address is required for connection")

    async def _resolve_device_while_scanning(self, *, timeout: float) -> Any:
        from bleak import BleakScanner

        target = self._bleak_client_target()
        if not isinstance(target, str):
            return target

        self._scanner = BleakScanner(bluez=cast(Any, self._bluez_args()))
        await self._scanner.start()
        deadline = asyncio.get_running_loop().time() + timeout
        target_upper = target.upper()
        while asyncio.get_running_loop().time() < deadline:
            for device in self._scanner.discovered_devices:
                if device.address.upper() == target_upper:
                    return device
            await asyncio.sleep(0.25)

        raise TimeoutError(f"Device {target} not seen on adapter {self._adapter or 'default'} during scan")

    async def connect(self, *, timeout: float = 10.0) -> None:
        from bleak import BleakClient

        initial_target = self._bleak_client_target()
        scanner_started = False

        try:
            if isinstance(initial_target, str):
                device = await self._resolve_device_while_scanning(timeout=timeout)
                scanner_started = True
            else:
                device = initial_target

            self._client = BleakClient(
                device,
                timeout=self._timeout,
                services=self._services,
                bluez=cast(Any, self._bluez_args()),
            )
            await self._client.connect(timeout=timeout)
        except Exception:
            if self._client is not None:
                with contextlib.suppress(Exception):
                    await self._client.disconnect()
                self._client = None
            raise
        finally:
            if scanner_started and self._scanner is not None:
                await self._scanner.stop()
                self._scanner = None

    async def disconnect(self) -> None:
        if self._client is not None:
            await self._client.disconnect()
            self._client = None

    @property
    def is_connected(self) -> bool:
        return bool(self._client is not None and self._client.is_connected)

    async def read_gatt_char(self, char_uuid: BluetoothUUID) -> bytes:
        raw_data = await self.client.read_gatt_char(str(char_uuid))
        return bytes(raw_data)

    async def write_gatt_char(self, char_uuid: BluetoothUUID, data: bytes, response: bool = True) -> None:
        await self.client.write_gatt_char(str(char_uuid), data, response=response)

    async def read_gatt_descriptor(self, desc_uuid: BluetoothUUID) -> bytes:
        raise NotImplementedError("Descriptor reads are not used by Thingy:52 GATT tests")

    async def write_gatt_descriptor(self, desc_uuid: BluetoothUUID, data: bytes) -> None:
        raise NotImplementedError("Descriptor writes are not used by Thingy:52 GATT tests")

    async def get_services(self) -> list[DeviceService]:
        return []

    async def start_notify(self, char_uuid: BluetoothUUID, callback: Callable[[str, bytes], None]) -> None:
        raise NotImplementedError("Notifications are not used by Thingy:52 GATT tests")

    async def stop_notify(self, char_uuid: BluetoothUUID) -> None:
        raise NotImplementedError("Notifications are not used by Thingy:52 GATT tests")

    async def pair(self) -> None:
        await self.client.pair()

    async def unpair(self) -> None:
        await self.client.unpair()

    async def read_rssi(self) -> int:
        raise NotImplementedError("Connection RSSI is not available through Bleak on Linux")

    async def get_advertisement_rssi(self, refresh: bool = False) -> int | None:
        return None

    def set_disconnected_callback(self, callback: Callable[[], None]) -> None:
        self._disconnected_callback = callback

    async def get_latest_advertisement(self, refresh: bool = False) -> AdvertisementData | None:
        return None

    @classmethod
    def convert_advertisement(cls, advertisement: object) -> AdvertisementData:
        raise NotImplementedError("Advertisement conversion is not used by Thingy:52 GATT tests")

    @property
    def mtu_size(self) -> int:
        return int(getattr(self.client, "mtu_size", 23))

    @property
    def name(self) -> str:
        return getattr(self._ble_device, "name", None) or self.address
