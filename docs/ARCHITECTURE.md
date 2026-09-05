# Architecture

## Firmware stack

| Component | Version or role |
| --- | --- |
| Arduino ESP32 | 2.0.17 framework |
| PlatformIO | Build and dependency management |
| LovyanGFX | ILI9341 display and XPT2046 touch |
| LVGL | Touch interface and widgets |
| ArduinoJson | Bounded JSON parsing and configuration |
| ESP32 WebServer | Setup, settings, artwork upload, and SD file manager |
| ESP32 DNSServer | Captive first-run setup |
| FreeRTOS | Serialized background network worker |

## Runtime model

LVGL, touch input, portal request handling, navigation, and page refreshes run
from the Arduino main loop. Page changes requested by a touch callback are
deferred until after `lv_timer_handler()`, preventing an active event from
destroying its own LVGL object tree.

A worker pinned to core 0 handles Weather, Flights, on-demand aircraft photos,
Bambuddy status and camera snapshots, and Systems jobs. One queue and
per-service busy flags prevent those requests from overlapping. A second core-0
worker handles update checks and OTA installation. Both workers share a network
mutex so the no-PSRAM device performs only one network operation at a time.
Snapshots are copied under their own mutexes before the UI reads them.

Detail pages use foreground-only refreshes: Weather refreshes Weather, Flights
refreshes Flights, Bambuddy refreshes Bambuddy, and Systems refreshes Systems.
The Home page refreshes enabled tile summaries sequentially. This prevents
unrelated network and SD work while a detailed monitor is open.

Replaceable 48x48 icons are validated and copied from the SD card into fixed
RAM buffers during startup. LVGL renders those memory-backed images rather
than holding FAT file handles open during redraws, so network cache activity
cannot invalidate an icon read.

## Memory strategy

The tested board has no PSRAM. The firmware therefore:

- Creates pages on demand instead of retaining every screen
- Uses a 32 KB LVGL allocator
- Keeps one synchronous display buffer covering five rows
- Bounds service responses and result counts
- Streams large flight responses through SD rather than a large RAM document
- Streams OTA firmware directly into the inactive application partition
- Uses a 16 KB network-worker stack and a 12 KB Arduino loop stack

## Storage

The SD card holds JSON configuration, connection secrets, themes, icons, and
temporary provider caches. On-demand aircraft and Bambuddy camera images each
use one bounded JPEG file that is replaced by the next successful lookup. All
SD access shares one FreeRTOS mutex. Firmware defaults allow the interface to
boot when the card or a file is missing.

NVS stores touch calibration, orientation-related state, tile visibility
overrides, and the generated bootstrap portal password.

## Networking

Public HTTPS providers use embedded root certificates and require a synchronized
clock before certificate validation. Open-Meteo is trusted at Let's Encrypt's
`Root YR` certificate so the no-PSRAM board does not have to verify the extra
4096-bit bridge to ISRG Root X1. Airport-Data's current P-384 issuer chain cannot
be verified within the original CYD's contiguous heap, so aircraft-photo
requests use encrypted but certificate-unverified transport. Those requests
contain no credentials, accept only exact Airport-Data URL prefixes, enforce
small response limits, and validate JPEG markers before display. SNTP starts
independently as soon as the station connects, using the configured POSIX
timezone. Wi-Fi sleep is disabled to improve request reliability. Service
failures are isolated to their own snapshots.

OTA checks authenticate the GitHub Releases API through the pinned GitHub
issuing CA. The API supplies the stable release version and the firmware
asset's expected size, exact download URL, and SHA-256 digest. The asset stream
may use an unverified CDN connection, but its bytes must match the authenticated
digest and size before activation. The installer rejects downgrades and writes
only the inactive application partition. On the next boot, startup completion
marks the pending image valid. A crash or restart before that checkpoint leaves
the bootloader able to select the prior OTA image.

As a low-memory fallback, the settings page can move release discovery and
download into the user's browser. GitHub's API supplies the official asset
digest to browser JavaScript, Web Crypto verifies the selected binary, and the
ESP32 verifies SHA-256 again while receiving the local upload. This removes
GitHub TLS, release JSON, and redirect handling from the no-PSRAM device.

When credentials are absent, the ESP32 starts a protected access point and
captive setup page. Successful setup writes both live JSON documents and
restarts. The access point is disabled as soon as station mode connects.

Systems HTTP checks deliberately use plain HTTP for small LAN health endpoints.
Bambuddy should also be treated as a trusted-LAN service unless a future build
adds configurable CA certificates.

## Portal safety

The portal uses Basic Auth and a per-boot form token. Credentials are write-only
in HTML forms. Configuration writes are staged, validated, backed up, and
recovered at boot. Startup artwork is size and dimension checked before atomic
replacement. File-manager paths are normalized and recursive deletion is not
supported.

## Source layout

| Path | Role |
| --- | --- |
| `firmware/src/main.cpp` | Device initialization, configuration, portal, LVGL pages, navigation, and scheduling |
| `firmware/src/live_data.cpp` | Provider clients, parsing, background worker, and thread-safe snapshots |
| `firmware/src/ota_update.cpp` | GitHub release checks, streamed OTA installation, and SHA-256 verification |
| `firmware/include/live_data.h` | Typed service settings and snapshot structures |
| `firmware/include/ota_update.h` | OTA state and thread-safe status snapshots |
| `firmware/include/firmware_version.h` | Installed release version shown by the device |
| `firmware/include/lgfx_cyd.h` | Display and touch hardware definition |
| `firmware/include/default_icons.h` | Embedded icon fallbacks |
| `firmware/include/default_startup.h` | Generated embedded startup JPEG |
| `firmware/src/lv_conf.h` | LVGL features, fonts, color depth, and memory pool |
| `tools/build_icons.py` | Converts source PNG artwork to LVGL alpha assets |
| `tools/build_startup.py` | Builds the SD startup JPEG and embedded fallback |
