"""Thingy:52 vendor GATT definitions for bluetooth_sig custom registration."""

from __future__ import annotations

from bluetooth_sig.gatt.characteristics.custom import CustomBaseCharacteristic
from bluetooth_sig.gatt.exceptions import InsufficientDataError
from bluetooth_sig.gatt.services.custom import CustomBaseGattService
from bluetooth_sig.types import CharacteristicInfo, ServiceInfo
from bluetooth_sig.types.uuid import BluetoothUUID

UPTIME_SERVICE_UUID = BluetoothUUID("00001900-0000-1000-8000-00805f9b34fb")
UPTIME_CHAR_UUID = BluetoothUUID("00001901-0000-1000-8000-00805f9b34fb")


class Thingy52UptimeCharacteristic(CustomBaseCharacteristic):
    """Device uptime in seconds (uint64 little-endian, Thingy:52 vendor characteristic)."""

    _info = CharacteristicInfo(
        uuid=UPTIME_CHAR_UUID,
        name="Uptime",
        unit="s",
        python_type=int,
    )
    expected_type = int
    expected_length = 8

    def _decode_value(
        self,
        data: bytearray,
        ctx: object | None = None,
        *,
        validate: bool = True,
    ) -> int:
        if len(data) != 8:
            raise InsufficientDataError(self.name, data, 8)
        return int.from_bytes(data, byteorder="little", signed=False)

    def _encode_value(self, value: int) -> bytearray:
        if value < 0:
            raise ValueError("uptime seconds must be non-negative")
        return bytearray(value.to_bytes(8, byteorder="little", signed=False))


class Thingy52UptimeService(CustomBaseGattService):
    """Thingy:52 vendor uptime GATT service."""

    _info = ServiceInfo(
        uuid=UPTIME_SERVICE_UUID,
        name="Thingy52 Uptime Service",
    )


def register_thingy52_custom_gatt() -> None:
    """Register vendor service/characteristic classes with the SIG translator singleton."""
    from bluetooth_sig import BluetoothSIGTranslator

    translator = BluetoothSIGTranslator.get_instance()
    Thingy52UptimeCharacteristic()
    translator.register_custom_service_class(
        str(UPTIME_SERVICE_UUID),
        Thingy52UptimeService,
        info=Thingy52UptimeService._info,
        override=True,
    )
