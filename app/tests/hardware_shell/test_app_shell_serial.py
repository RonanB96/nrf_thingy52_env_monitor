"""HIL over an already-flashed shell image. Skips unless SERIAL_PORT is set.

Flash first:
  west build -p always app -d app/build -- -DEXTRA_CONF_FILE=shell.conf
  west flash -d app/build --runner jlink

Then:
  eval "$(scripts/resolve_serial_port.sh --export)"
  pytest app/tests/hardware_shell/test_app_shell_serial.py
"""

from __future__ import annotations

import os
from collections.abc import Generator

import pytest

from shell_hil_cases import (
    assert_ccc_temp_toggles_ess_status,
    assert_connect_disconnect_updates_counts,
    assert_help_lists_gatt,
    assert_nested_connect_count,
    assert_poll_after_connect,
    assert_unknown_characteristic_is_rejected,
    reset_simulated_connections,
)
from shell_serial import SerialShell

SERIAL_PORT = os.environ.get("SERIAL_PORT", "")

pytestmark = pytest.mark.skipif(not SERIAL_PORT, reason="SERIAL_PORT not set")


@pytest.fixture(scope="module")
def shell() -> Generator[SerialShell, None, None]:
    session = SerialShell(SERIAL_PORT)
    try:
        if not session.wait_for_prompt(timeout=20.0):
            pytest.fail(f"shell prompt not found on {SERIAL_PORT}")
        yield session
    finally:
        session.close()


@pytest.fixture(autouse=True)
def _reset_simulated_connections(shell: SerialShell) -> None:
    reset_simulated_connections(shell)


def test_serial_help_lists_gatt(shell: SerialShell) -> None:
    assert_help_lists_gatt(shell)


def test_serial_connect_disconnect_updates_counts(shell: SerialShell) -> None:
    assert_connect_disconnect_updates_counts(shell)


def test_serial_nested_connect_count(shell: SerialShell) -> None:
    assert_nested_connect_count(shell)


def test_serial_poll_after_connect(shell: SerialShell) -> None:
    assert_poll_after_connect(shell)


def test_serial_ccc_temp_toggles_ess_status(shell: SerialShell) -> None:
    assert_ccc_temp_toggles_ess_status(shell)


def test_serial_unknown_characteristic_is_rejected(shell: SerialShell) -> None:
    assert_unknown_characteristic_is_rejected(shell)
