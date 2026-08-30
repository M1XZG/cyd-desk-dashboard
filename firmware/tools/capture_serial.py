import argparse
import sys
import time

import serial


def main() -> int:
    parser = argparse.ArgumentParser(description="Capture ESP32 diagnostic serial output.")
    parser.add_argument("--port", default="COM4")
    parser.add_argument("--baud", default=115200, type=int)
    parser.add_argument("--duration", default=25, type=float)
    args = parser.parse_args()

    connection = serial.Serial()
    connection.port = args.port
    connection.baudrate = args.baud
    connection.timeout = 0.25
    connection.dsrdtr = False
    connection.rtscts = False
    connection.dtr = False
    connection.rts = False

    with connection:
        deadline = time.monotonic() + args.duration

        while time.monotonic() < deadline:
            chunk = connection.read(connection.in_waiting or 1)
            if chunk:
                sys.stdout.buffer.write(chunk)
                sys.stdout.buffer.flush()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
