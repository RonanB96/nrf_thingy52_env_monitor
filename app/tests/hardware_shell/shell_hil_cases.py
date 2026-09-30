"""HIL assertions for the GATT-path UART shell.

CCS811 may still be in the 20-minute boot conditioning window; do not assert
idle after disconnect.
"""

from __future__ import annotations

import time

from shell_parse import (
    command_text,
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

CONNECT_TIMEOUT_S = 45.0
POLL_TIMEOUT_S = 45.0


def wait_until_armed(shell: object, timeout: float = 30.0) -> None:
    deadline = time.time() + timeout
    last_error: str | None = None
    while time.time() < deadline:
        try:
            status = parse_sm_status(command_text(shell, "app sm status", timeout=5.0))
        except ValueError as exc:
            last_error = str(exc)
            time.sleep(0.5)
            continue
        if status.armed:
            return
        time.sleep(0.5)
    raise AssertionError(f"sensor manager did not arm within {timeout}s: {last_error}")


def reset_simulated_connections(shell: object) -> None:
    wait_until_armed(shell)
    status = parse_sm_status(command_text(shell, "app sm status"))
    while status.count > 0:
        command_text(shell, "app gatt disconnect")
        status = parse_sm_status(command_text(shell, "app sm status"))


def assert_help_lists_gatt(shell: object) -> None:
    text = command_text(shell, "app", timeout=10.0)
    assert "gatt" in text
    assert "sm" in text
    assert "ccs811" in text
    assert "ess" in text


def assert_connect_disconnect_updates_counts(shell: object) -> None:
    idle = parse_sm_status(command_text(shell, "app sm status"))
    assert idle.armed is True
    assert idle.count == 0

    connected = parse_connect_count(
        command_text(shell, "app gatt connect", timeout=CONNECT_TIMEOUT_S)
    )
    assert connected == 1
    assert parse_sm_status(command_text(shell, "app sm status")).count == 1

    ccs811 = parse_ccs811_status(command_text(shell, "app ccs811 status"))
    assert ccs811.enabled is True
    assert ccs811.ble_connected is True
    # ready stays false for up to 20 minutes of boot conditioning.
    if ccs811.cond_ms > 0:
        assert ccs811.ready is False
    else:
        assert ccs811.ready is True

    disconnected = parse_disconnect_count(command_text(shell, "app gatt disconnect"))
    assert disconnected == 0
    assert parse_sm_status(command_text(shell, "app sm status")).count == 0
    assert parse_ccs811_status(command_text(shell, "app ccs811 status")).ble_connected is False


def assert_nested_connect_count(shell: object) -> None:
    assert parse_connect_count(
        command_text(shell, "app gatt connect", timeout=CONNECT_TIMEOUT_S)
    ) == 1
    assert parse_connect_count(
        command_text(shell, "app gatt connect", timeout=CONNECT_TIMEOUT_S)
    ) == 2
    assert parse_sm_status(command_text(shell, "app sm status")).count == 2
    assert parse_ccs811_status(command_text(shell, "app ccs811 status")).ble_connected is True

    assert parse_disconnect_count(command_text(shell, "app gatt disconnect")) == 1
    assert parse_ccs811_status(command_text(shell, "app ccs811 status")).ble_connected is True
    assert parse_disconnect_count(command_text(shell, "app gatt disconnect")) == 0
    assert parse_ccs811_status(command_text(shell, "app ccs811 status")).ble_connected is False


def assert_poll_after_connect(shell: object) -> None:
    parse_connect_count(command_text(shell, "app gatt connect", timeout=CONNECT_TIMEOUT_S))
    try:
        poll = command_text(shell, "app gatt poll", timeout=POLL_TIMEOUT_S)
        names = {read.name for read in parse_wire_reads(poll)}
        assert names == {"temp", "humidity", "pressure", "co2", "tvoc"}
        level, _charging = parse_battery(poll)
        assert 0 <= level <= 100

        temp_c = parse_decoded_temp_c(command_text(shell, "app gatt read temp", timeout=15.0))
        assert 0.0 < temp_c < 50.0

        humidity = parse_decoded_humidity_pct(
            command_text(shell, "app gatt read humidity", timeout=15.0)
        )
        assert 0.0 <= humidity <= 100.0

        pressure_hpa = parse_decoded_pressure_hpa(
            command_text(shell, "app gatt read pressure", timeout=15.0)
        )
        assert 300.0 < pressure_hpa < 1100.0

        level, _charging = parse_battery(command_text(shell, "app gatt read battery"))
        assert 0 <= level <= 100

        uptime_s = parse_uptime_seconds(command_text(shell, "app gatt read uptime"))
        assert uptime_s >= 0
    finally:
        command_text(shell, "app gatt disconnect")


def assert_ccc_temp_toggles_ess_status(shell: object) -> None:
    char, enabled = parse_ccc(command_text(shell, "app gatt ccc temp on"))
    assert char == "temp"
    assert enabled is True
    status = parse_ess_status(command_text(shell, "app ess status"))
    assert status["temp"].notify is True

    char, enabled = parse_ccc(command_text(shell, "app gatt ccc temp off"))
    assert char == "temp"
    assert enabled is False
    status = parse_ess_status(command_text(shell, "app ess status"))
    assert status["temp"].notify is False


def assert_unknown_characteristic_is_rejected(shell: object) -> None:
    text = command_text(shell, "app gatt read bogon", timeout=10.0)
    assert has_unknown_characteristic_error(text)
