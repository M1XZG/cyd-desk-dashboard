#!/usr/bin/env python3

from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE_PATH = ROOT / "assets/startup-logo.png"
OUTPUT_PATH = ROOT / "sd-card/dashboard/startup.jpg"
HEADER_PATH = ROOT / "firmware/include/default_startup.h"
WIDTH = 320
HEIGHT = 240


def convert_image() -> bytes:
    image = Image.open(SOURCE_PATH).convert("RGB")
    if image.size != (WIDTH, HEIGHT):
        raise ValueError(
            f"expected {WIDTH}x{HEIGHT}, got {image.width}x{image.height}"
        )

    image.save(
        OUTPUT_PATH,
        format="JPEG",
        quality=90,
        optimize=True,
        subsampling=0,
    )
    return OUTPUT_PATH.read_bytes()


def write_header(data: bytes) -> None:
    with HEADER_PATH.open("w", newline="\n") as header:
        header.write("#pragma once\n\n#include <Arduino.h>\n\n")
        header.write("static const uint8_t kDefaultStartupJpeg[] PROGMEM = {\n")
        for offset in range(0, len(data), 16):
            row = data[offset:offset + 16]
            values = ", ".join(f"0x{value:02X}" for value in row)
            header.write(f"    {values},\n")
        header.write("};\n")


def main() -> None:
    data = convert_image()
    write_header(data)
    print(
        f"Wrote {OUTPUT_PATH.relative_to(ROOT)} and "
        f"{HEADER_PATH.relative_to(ROOT)}"
    )


if __name__ == "__main__":
    main()
