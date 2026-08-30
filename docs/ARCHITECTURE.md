# Architecture

## Firmware stack

| Component | Version or role |
| --- | --- |
| Arduino ESP32 | 2.0.17 framework |
| PlatformIO | Build and dependency management |
| LovyanGFX | ILI9341 display and XPT2046 touch |
| LVGL | Touch interface and widgets |
| ArduinoJson | Bounded JSON parsing and configuration |
| ESP32 WebServer | Settings portal and SD file manager |
| FreeRTOS | Serialized background network worker |

## Runtime model

LVGL, touch input, portal request handling, navigation, and page refreshes run
from the Arduino main loop. Page changes requested by a touch callback are
deferred until after `lv_timer_handler()`, preventing an active event from
destroying its own LVGL object tree.

A single worker pinned to core 0 handles Weather, Flights, Bambuddy, and
Systems jobs. One queue and per-service busy flags prevent overlapping requests.
Snapshots are copied under a mutex before the UI reads them.

## Memory strategy

The tested board has no PSRAM. The firmware therefore:

- Creates pages on demand instead of retaining every screen
- Uses a 40 KB LVGL allocator
- Keeps two display buffers covering ten rows each
- Bounds service responses and result counts
- Streams large flight responses through SD rather than a large RAM document
- Uses a 16 KB network-worker stack and a 12 KB Arduino loop stack

## Storage

The SD card holds JSON configuration, connection secrets, themes, icons, and
temporary provider caches. All SD access shares one FreeRTOS mutex. Firmware
defaults allow the interface to boot when the card or a file is missing.

NVS stores touch calibration, orientation-related state, tile visibility
overrides, and the generated bootstrap portal password.

## Networking

Public HTTPS providers use embedded root certificates and require a synchronized
clock before certificate validation. Wi-Fi sleep is disabled to improve
request reliability. Service failures are isolated to their own snapshots.

Systems HTTP checks deliberately use plain HTTP for small LAN health endpoints.
Bambuddy should also be treated as a trusted-LAN service unless a future build
adds configurable CA certificates.

## Portal safety

The portal uses Basic Auth and a per-boot form token. Credentials are write-only
in HTML forms. Configuration writes are staged, validated, backed up, and
recovered at boot. File-manager paths are normalized and recursive deletion is
not supported.

## Source layout

| Path | Role |
| --- | --- |
| `firmware/src/main.cpp` | Device initialization, configuration, portal, LVGL pages, navigation, and scheduling |
| `firmware/src/live_data.cpp` | Provider clients, parsing, background worker, and thread-safe snapshots |
| `firmware/include/live_data.h` | Typed service settings and snapshot structures |
| `firmware/include/lgfx_cyd.h` | Display and touch hardware definition |
| `firmware/include/default_icons.h` | Embedded icon fallbacks |
| `firmware/src/lv_conf.h` | LVGL features, fonts, color depth, and memory pool |
| `tools/build_icons.py` | Converts source PNG artwork to LVGL alpha assets |
