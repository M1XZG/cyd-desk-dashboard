# CYD Desk Dashboard

A self-contained touchscreen information dashboard for the
ESP32-2432S028, commonly sold as the Cheap Yellow Display or CYD.

The firmware runs directly on the ESP32 without a companion process for its
public weather and flight data. Optional integrations such as Bambuddy still
require their service to be reachable on the local network. Configuration is
managed through an authenticated browser portal, with settings and replaceable
artwork stored on a microSD card.

![CYD Desk Dashboard home screen](docs/images/home-screen.jpg)

## Highlights

- 320x240 landscape touch interface with a six-tile home screen
- Live weather from Open-Meteo, with MET Norway and sunrise-sunset.org fallbacks
- Standalone nearby-aircraft radar using airplanes.live, adsb.lol, or the
  official Flightradar24 API
- Read-only Bambuddy printer monitoring
- Device, internet, HTTP endpoint, and TCP port health checks
- Sunday-first monthly calendar with previous and next month navigation
- Authenticated browser settings portal and SD-card file manager
- Replaceable SD-card icons with built-in fallbacks
- Branded startup screen with an SD-card JPEG override
- Saved touch calibration, orientation, brightness, and tile visibility
- Serialized network and SD work to keep the interface responsive

## Start here

1. Read the [hardware and SD-card requirements](docs/HARDWARE.md).
2. Follow the [installation and first-run guide](docs/GETTING-STARTED.md).
3. Configure services through the
   [browser portal](docs/WEB-PORTAL.md).
4. Use the [configuration reference](docs/CONFIGURATION.md) for manual SD-card
   setup or recovery.

## Documentation

| Guide | Contents |
| --- | --- |
| [Features](docs/FEATURES.md) | Every screen, data source, control, and fallback |
| [Getting started](docs/GETTING-STARTED.md) | Build, flash, SD preparation, calibration, and first login |
| [Hardware](docs/HARDWARE.md) | Supported board, pin use, storage, and power expectations |
| [Web portal](docs/WEB-PORTAL.md) | Portal sections, credentials, saves, and file manager |
| [Configuration](docs/CONFIGURATION.md) | JSON files, accepted values, secrets, and examples |
| [Architecture](docs/ARCHITECTURE.md) | Tasks, memory, rendering, networking, persistence, and safety |
| [Troubleshooting](docs/TROUBLESHOOTING.md) | Common display, touch, Wi-Fi, SD, and service problems |
| [Screenshots](docs/SCREENSHOTS.md) | Device and browser interface image index |
| [Roadmap](docs/ROADMAP.md) | Planned OTA, tile ordering, and startup personalization |
| [Asset sources](ASSET-SOURCES.md) | Artwork provenance and third-party licences |
| [Contributing](CONTRIBUTING.md) | Development workflow and pull-request expectations |
| [Security](SECURITY.md) | Credential handling, LAN exposure, and reporting |

## Current status

The firmware has been built and tested on an ESP32-D0WD-V3 CYD with 4 MB
flash and no PSRAM. Weather, Flights, Bambuddy, Systems, Calendar, the settings
portal, SD file management, touch calibration, and month navigation have all
run on the physical device.

The current build metadata is recorded in
[`firmware/BUILD-INFO.txt`](firmware/BUILD-INFO.txt).

## License

The firmware and project documentation are released under the
[MIT License](LICENSE). The bundled Lucide icons retain their
[ISC license](assets/icons/LUCIDE-LICENSE.txt). Third-party board reference
images are included with permission and are not covered by the MIT licence.
