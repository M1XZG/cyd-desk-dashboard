#!/usr/bin/env python3

from pathlib import Path
import struct

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "sd-card/dashboard/assets/icons"
OUTPUT_DIR = ROOT / "sd-card/dashboard/icons"
HEADER_PATH = ROOT / "firmware/include/default_icons.h"
ICON_SOURCES = {
    "Weather": "weather",
    "Flights": "flights",
    "Bambuddy": "bambuddy",
    "Systems": "systems",
    "Calendar": "calendar",
    "Settings": "settings",
    "Sun": "sun",
    "Moon": "moon",
    "CloudMoon": "cloud-moon",
    "Cloud": "cloud",
    "Fog": "cloud-fog",
    "Rain": "cloud-rain",
    "Snow": "cloud-snow",
    "Thunderstorm": "cloud-lightning",
}
WIDTH = 48
HEIGHT = 48
LV_IMG_CF_ALPHA_4BIT = 13


def convert_icon(name: str) -> bytes:
    image = Image.open(SOURCE_DIR / f"{ICON_SOURCES[name]}.png").convert("LA")
    if image.size != (WIDTH, HEIGHT):
        raise ValueError(f"{name}: expected {WIDTH}x{HEIGHT}, got {image.size}")

    alpha = image.getchannel("A").tobytes()
    pixels = bytearray()
    for index in range(0, len(alpha), 2):
        high = round(alpha[index] * 15 / 255)
        low = round(alpha[index + 1] * 15 / 255)
        pixels.append((high << 4) | low)

    header = (
        LV_IMG_CF_ALPHA_4BIT
        | (WIDTH << 10)
        | (HEIGHT << 21)
    )
    return struct.pack("<I", header) + pixels


def write_header(assets: dict[str, bytes]) -> None:
    with HEADER_PATH.open("w", newline="\n") as header:
        header.write("#pragma once\n\n#include <Arduino.h>\n\n")
        for name, data in assets.items():
            symbol = f"kDefault{name}Icon"
            header.write(f"static const uint8_t {symbol}[] PROGMEM = {{\n")
            for offset in range(0, len(data), 16):
                row = data[offset:offset + 16]
                values = ", ".join(f"0x{value:02X}" for value in row)
                header.write(f"    {values},\n")
            header.write("};\n\n")

        header.write(
            "struct DefaultIconAsset {\n"
            "  const char* filename;\n"
            "  const uint8_t* data;\n"
            "  size_t size;\n"
            "};\n\n"
            "static const DefaultIconAsset kDefaultIconAssets[] = {\n"
        )
        for name in assets:
            header.write(
                f'    {{"{name}.bin", kDefault{name}Icon, '
                f"sizeof(kDefault{name}Icon)}},\n"
            )
        header.write("};\n")


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    assets = {name: convert_icon(name) for name in ICON_SOURCES}
    for name, data in assets.items():
        (OUTPUT_DIR / f"{name}.bin").write_bytes(data)
    write_header(assets)
    print(f"Wrote {len(assets)} icons and {HEADER_PATH.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
