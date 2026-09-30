"""pyserial stand-in for twister_harness.Shell when firmware is already flashed."""

from __future__ import annotations

import re
import time

import serial


class SerialShell:
    """Minimal Shell-compatible helper over an already-open UART."""

    def __init__(self, port: str, baud: int = 115200, prompt: str = "uart:~$", timeout: float = 30.0):
        self.prompt = prompt
        self.base_timeout = timeout
        self._ser = serial.Serial(port, baud, timeout=0.2)
        self._ser.dtr = True
        self._ser.rts = True
        self._ser.reset_input_buffer()

    def close(self) -> None:
        self._ser.close()

    def wait_for_prompt(self, timeout: float | None = None) -> bool:
        timeout = timeout or self.base_timeout
        deadline = time.time() + timeout
        self._ser.reset_input_buffer()
        while time.time() < deadline:
            self._ser.write(b"\n")
            line = self._readline(0.5)
            if line is not None and self.prompt in line:
                return True
        return False

    def exec_command(self, command: str, timeout: float | None = None, print_output: bool = True) -> list[str]:
        del print_output
        timeout = timeout or self.base_timeout
        self._ser.reset_input_buffer()
        self._ser.write(f"{command}\n\n".encode())
        regex_command = re.compile(f".*{re.escape(command)}")
        regex_prompt = re.compile(re.escape(self.prompt))
        lines: list[str] = []
        lines.extend(self._readlines_until(regex_command, 1.0))
        lines.extend(self._readlines_until(regex_prompt, timeout))
        return lines

    def get_filtered_output(self, command_lines: list[str]) -> list[str]:
        regex_filter = re.compile("|".join([re.escape(self.prompt), "<dbg>", "<inf>", "<wrn>", "<err>"]))
        return [line for line in command_lines if not regex_filter.search(line)]

    def _readline(self, timeout: float) -> str | None:
        deadline = time.time() + timeout
        buf = b""
        while time.time() < deadline:
            chunk = self._ser.read(1)
            if not chunk:
                continue
            buf += chunk
            if buf.endswith(b"\n"):
                return buf.decode("utf-8", errors="replace").strip()
        if buf:
            return buf.decode("utf-8", errors="replace").strip()
        return None

    def _readlines_until(self, regex: re.Pattern[str], timeout: float) -> list[str]:
        deadline = time.time() + timeout
        lines: list[str] = []
        while time.time() < deadline:
            remaining = max(0.05, deadline - time.time())
            line = self._readline(min(0.5, remaining))
            if line is None:
                continue
            lines.append(line)
            if regex.search(line):
                return lines
        raise TimeoutError(f"timeout waiting for {regex.pattern!r}; got {lines!r}")
