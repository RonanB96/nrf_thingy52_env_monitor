#!/usr/bin/env python3
"""Persistent serial logger. Reads forever until killed (SIGTERM/SIGINT).

Usage: python3 scripts/serial_logger.py <port> <baud> <log_file>

Writes every received line to stdout and to log_file.
"""

import serial
import signal
import sys

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

    s = serial.Serial(port, baud, timeout=0.5)
    s.reset_input_buffer()

    with open(log_file, 'w') as f:
        while running:
            line = s.readline()
            if line:
                text = line.decode('utf-8', errors='replace').strip()
                print(text, flush=True)
                f.write(text + '\n')
                f.flush()

    s.close()

if __name__ == '__main__':
    main()
