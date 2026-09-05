import argparse
import sys
import time

import serial
from serial.tools import list_ports


KNOWN_ESP32_USB_ADAPTERS = {
    (0x1A86, 0x7523),  # CH340
}


def detect_port() -> str:
    ports = list(list_ports.comports())
    known_ports = [
        port.device
        for port in ports
        if (port.vid, port.pid) in KNOWN_ESP32_USB_ADAPTERS
    ]

    if len(known_ports) == 1:
        return known_ports[0]
    if not known_ports and len(ports) == 1:
        return ports[0].device
    if not ports:
        raise RuntimeError("No serial ports were detected")

    choices = ", ".join(port.device for port in ports)
    raise RuntimeError(
        f"Could not identify one ESP32 serial port; use --port with one of: {choices}"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Capture ESP32 diagnostic serial output.")
    parser.add_argument("--port", help="Serial port; auto-detected when omitted")
    parser.add_argument("--baud", default=115200, type=int)
    parser.add_argument("--duration", default=25, type=float)
    args = parser.parse_args()

    if args.port is None:
        try:
            args.port = detect_port()
        except RuntimeError as error:
            parser.error(str(error))
        print(f"Using auto-detected serial port {args.port}", file=sys.stderr)

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
