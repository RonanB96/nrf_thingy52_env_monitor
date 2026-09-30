"""Parse GATT-path debug shell output. Shared by HIL and offline tests."""

from __future__ import annotations

import re
from dataclasses import dataclass

SM_STATUS_RE = re.compile(r"armed=(yes|no) count=(\d+)")
CONNECT_RE = re.compile(r"simulated GATT connect \(count=(\d+)\)")
DISCONNECT_RE = re.compile(r"simulated GATT disconnect \(count=(\d+)\)")
WIRE_RE = re.compile(
    r"^(temp|humidity|pressure|co2|tvoc) wire \[(\d+)\]: ([0-9a-fA-F ]+)$",
    re.MULTILINE,
)
CCS811_RE = re.compile(
    r"ready=(yes|no) enabled=(yes|no) idle=(yes|no) ble_connected=(yes|no) "
    r"await_1s=(yes|no) mode=(-?\d+) cond_ms=(\d+)"
)
CCC_RE = re.compile(
    r"^(temp|humidity|pressure|co2|tvoc) CCC notify (on|off)$",
    re.MULTILINE,
)
ESS_STATUS_RE = re.compile(
    r"^(temp|humidity|pressure|co2|tvoc) known=(yes|no) notify=(yes|no) value=(-?\d+)$",
    re.MULTILINE,
)
BATTERY_RE = re.compile(r"battery cache: (\d+)% charging=(yes|no)")
DECODED_TEMP_RE = re.compile(r"decoded: (-?\d+) \((-?\d+\.\d+) C\)")
DECODED_HUM_RE = re.compile(r"decoded: (\d+) \((\d+\.\d+) %\)")
DECODED_PRESS_RE = re.compile(r"decoded: (\d+) \((\d+\.\d+) hPa\)")
DECODED_CO2_RE = re.compile(r"decoded: (0xFFFF \(unknown\)|(\d+) ppm)")
DECODED_TVOC_RE = re.compile(r"decoded: (0xFFFF \(unknown\)|(\d+) ppb)")
UPTIME_RE = re.compile(r"decoded: (\d+) s")
UNKNOWN_CHAR_RE = re.compile(r"unknown characteristic")


def _yes(value: str) -> bool:
    return value == "yes"


@dataclass(frozen=True)
class SmStatus:
    armed: bool
    count: int


@dataclass(frozen=True)
class Ccs811Status:
    ready: bool
    enabled: bool
    idle: bool
    ble_connected: bool
    await_first_1s: bool
    mode: int
    cond_ms: int


@dataclass(frozen=True)
class EssCharStatus:
    name: str
    known: bool
    notify: bool
    value: int


@dataclass(frozen=True)
class WireRead:
    name: str
    nbytes: int
    hex_bytes: bytes


def parse_sm_status(text: str) -> SmStatus:
    match = SM_STATUS_RE.search(text)
    if match is None:
        raise ValueError(f"sm status not found in: {text!r}")
    return SmStatus(armed=_yes(match.group(1)), count=int(match.group(2)))


def parse_connect_count(text: str) -> int:
    match = CONNECT_RE.search(text)
    if match is None:
        raise ValueError(f"connect confirmation not found in: {text!r}")
    return int(match.group(1))


def parse_disconnect_count(text: str) -> int:
    match = DISCONNECT_RE.search(text)
    if match is None:
        raise ValueError(f"disconnect confirmation not found in: {text!r}")
    return int(match.group(1))


def parse_ccs811_status(text: str) -> Ccs811Status:
    match = CCS811_RE.search(text)
    if match is None:
        raise ValueError(f"ccs811 status not found in: {text!r}")
    return Ccs811Status(
        ready=_yes(match.group(1)),
        enabled=_yes(match.group(2)),
        idle=_yes(match.group(3)),
        ble_connected=_yes(match.group(4)),
        await_first_1s=_yes(match.group(5)),
        mode=int(match.group(6)),
        cond_ms=int(match.group(7)),
    )


def parse_wire_reads(text: str) -> list[WireRead]:
    reads: list[WireRead] = []
    for match in WIRE_RE.finditer(text):
        raw = bytes.fromhex(match.group(3))
        reads.append(WireRead(name=match.group(1), nbytes=int(match.group(2)), hex_bytes=raw))
    return reads


def parse_ess_status(text: str) -> dict[str, EssCharStatus]:
    found: dict[str, EssCharStatus] = {}
    for match in ESS_STATUS_RE.finditer(text):
        found[match.group(1)] = EssCharStatus(
            name=match.group(1),
            known=_yes(match.group(2)),
            notify=_yes(match.group(3)),
            value=int(match.group(4)),
        )
    return found


def parse_ccc(text: str) -> tuple[str, bool]:
    match = CCC_RE.search(text)
    if match is None:
        raise ValueError(f"CCC confirmation not found in: {text!r}")
    return match.group(1), match.group(2) == "on"


def parse_battery(text: str) -> tuple[int, bool]:
    match = BATTERY_RE.search(text)
    if match is None:
        raise ValueError(f"battery cache not found in: {text!r}")
    return int(match.group(1)), _yes(match.group(2))


def parse_decoded_temp_c(text: str) -> float:
    match = DECODED_TEMP_RE.search(text)
    if match is None:
        raise ValueError(f"decoded temperature not found in: {text!r}")
    return float(match.group(2))


def parse_decoded_humidity_pct(text: str) -> float:
    match = DECODED_HUM_RE.search(text)
    if match is None:
        raise ValueError(f"decoded humidity not found in: {text!r}")
    return float(match.group(2))


def parse_decoded_pressure_hpa(text: str) -> float:
    match = DECODED_PRESS_RE.search(text)
    if match is None:
        raise ValueError(f"decoded pressure not found in: {text!r}")
    return float(match.group(2))


def parse_uptime_seconds(text: str) -> int:
    match = UPTIME_RE.search(text)
    if match is None:
        raise ValueError(f"decoded uptime not found in: {text!r}")
    return int(match.group(1))


def has_unknown_characteristic_error(text: str) -> bool:
    return UNKNOWN_CHAR_RE.search(text) is not None


def command_text(shell_like: object, command: str, timeout: float = 30.0) -> str:
    """Run a shell command and return filtered printable output."""
    exec_command = getattr(shell_like, "exec_command")
    lines = exec_command(command, timeout=timeout)
    get_filtered = getattr(shell_like, "get_filtered_output", None)
    if callable(get_filtered):
        lines = get_filtered(lines)
    return "\n".join(lines)
