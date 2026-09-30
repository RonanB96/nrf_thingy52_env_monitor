"""Twister HIL: drive the GATT-path UART shell on the Thingy:52."""

from __future__ import annotations

import sys
from pathlib import Path
from typing import TYPE_CHECKING

import pytest

_ROOT = Path(__file__).resolve().parents[1]
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

from shell_hil_cases import (  # noqa: E402
    assert_ccc_temp_toggles_ess_status,
    assert_connect_disconnect_updates_counts,
    assert_help_lists_gatt,
    assert_nested_connect_count,
    assert_poll_after_connect,
    assert_unknown_characteristic_is_rejected,
    reset_simulated_connections,
)

if TYPE_CHECKING:
    from twister_harness import Shell


@pytest.fixture(autouse=True)
def _reset_simulated_connections(shell: "Shell") -> None:
    reset_simulated_connections(shell)


def test_app_help_lists_gatt(shell: "Shell") -> None:
    assert_help_lists_gatt(shell)


def test_connect_disconnect_updates_counts(shell: "Shell") -> None:
    assert_connect_disconnect_updates_counts(shell)


def test_nested_connect_count(shell: "Shell") -> None:
    assert_nested_connect_count(shell)


def test_poll_after_connect_returns_ess_wires(shell: "Shell") -> None:
    assert_poll_after_connect(shell)


def test_ccc_temp_toggles_ess_status(shell: "Shell") -> None:
    assert_ccc_temp_toggles_ess_status(shell)


def test_unknown_characteristic_is_rejected(shell: "Shell") -> None:
    assert_unknown_characteristic_is_rejected(shell)
