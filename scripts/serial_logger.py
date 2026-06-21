#!/usr/bin/env python3
"""Persistent serial logger. Reads forever until killed (SIGTERM/SIGINT).

Usage: python3 scripts/serial_logger.py <port> <baud> <log_file>

Writes every received line to stdout and to log_file.
"""

import serial
import signal
import sys
import time

def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <port> <baud> <log_file>", file=sys.stderr)
        sys.exit(1)

    port, baud, log_file = sys.argv[1], int(sys.argv[2]), sys.argv[3]

    running = True

    def handle_signal(signum, frame):
        nonlocal running
        running = False

    signal.signal(signal.SIGTERM, handle_signal)
    signal.signal(signal.SIGINT, handle_signal)

    def open_port():
        s = serial.Serial(port, baud, timeout=0.5)
        s.dtr = True
        s.rts = True
        s.reset_input_buffer()
        return s

    s = open_port()

    with open(log_file, 'w') as f:
        while running:
            try:
                line = s.readline()
            except serial.SerialException:
                if not running:
                    break
                time.sleep(0.1)
                try:
                    s.close()
                except serial.SerialException:
                    pass
                try:
                    s = open_port()
                except serial.SerialException:
                    time.sleep(0.2)
                continue

            if line:
                text = line.decode('utf-8', errors='replace').strip()
                print(text, flush=True)
                f.write(text + '\n')
                f.flush()

    s.close()

if __name__ == '__main__':
    main()
