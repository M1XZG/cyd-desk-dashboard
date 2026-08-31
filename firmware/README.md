# Firmware

This PlatformIO project targets the ESP32-2432S028 CYD described in
[`../docs/HARDWARE.md`](../docs/HARDWARE.md).

## Build

```bash
pio run -e cyd
```

The application binary is written to `.pio/build/cyd/firmware.bin`. Serial
diagnostics use 115200 baud.

## Upload

```bash
pio run -e cyd -t upload --upload-port /dev/ttyUSB0
```

Use the board's COM port on Windows. See the full
[getting-started guide](../docs/GETTING-STARTED.md) before flashing a new board.

Existing installations running `v1.1.0` or later can install stable tagged
releases without USB. The version in `include/firmware_version.h` must match
the release tag. Follow the build, tag, publication, device installation, and
rollback checks in the [OTA update guide](../docs/OTA-UPDATES.md).

## Source

`src/main.cpp` owns initialization, configuration, the web portal, LVGL pages,
touch handling, and refresh scheduling. `src/live_data.cpp` owns the serialized
network worker and live-service parsers. Shared data structures are declared in
`include/live_data.h`.

The board has no PSRAM. Keep new network responses bounded, route SD access
through the shared mutex, and defer page changes until after the active LVGL
event callback.
