# Feature reference

## Home screen

The home screen is a 3x2 landscape tile grid. Weather, Flights, Bambuddy,
Systems, and Calendar can be hidden. Settings remains available so a disabled
tile can be restored. Each tile uses a 48x48 icon from the SD card and falls
back to a built-in copy or text initial if the file cannot be read.

![CYD Desk Dashboard home screen](images/home-screen.jpg)

## Startup screen

The dashboard opens with a 320x240 project graphic. The embedded standard
artwork remains visible for at least 1.2 seconds. A valid custom
`/dashboard/startup.jpg` remains visible for four seconds so the personalized
screen can be seen before the Home page appears. The JPEG must be a 320x240
baseline RGB image no larger than 200 KB. The browser portal provides a
dedicated validated upload, and the embedded artwork is used if the SD file
cannot be displayed.

![CYD Desk Dashboard startup screen](images/startup-screen.jpg)

## Weather

Weather searches Open-Meteo by town or postcode, then requests current and
daily conditions for the selected coordinates. If the public forecast endpoint
is rate-limited or cannot be reached, the dashboard temporarily uses MET
Norway's Locationforecast service. Sunrise, sunset, and moon data then come
from sunrise-sunset.org. The active data sources are credited on the Weather
page. The page shows:

- Current temperature and apparent temperature
- Weather condition with a matching day or night icon
- Daily high and low
- Humidity, wind speed, and pressure
- Sunrise and sunset
- Approximate moon phase and illumination

Temperature, wind, and pressure units are configurable.

![Weather conditions and astronomy screen](images/weather-screen.jpg)

Swipe upward to reveal the provider credit while keeping the detailed
conditions and astronomy visible.

![Weather details with Open-Meteo provider credit](images/weather-details-screen.jpg)

## Flights

Flights works without another computer or local receiver. It supports:

| Provider | Credential | Notes |
| --- | --- | --- |
| airplanes.live | None | Default point-and-radius feed |
| adsb.lol | None | Compatible alternative |
| Flightradar24 | API token | Requires an official subscription |

The device downloads the regional response to SD, parses aircraft one at a
time, calculates distance and bearing, sorts by distance, and keeps the nearest
configured aircraft. The radar is north-up. Symbols show heading and use color
to distinguish altitude or vertical movement.

![Nearby-aircraft radar screen](images/flights-screen.jpg)

Tapping an aircraft opens a detail page with the fields supplied by the
provider: callsign, registration, aircraft type, route, altitude, speed,
heading, distance, bearing, and climb or descent rate. The page then looks up
one public thumbnail from Airport-Data.com by ICAO hex code. When available,
the photo is downloaded on demand, fitted into a 150x100 frame, and shown with
the photographer and source credit. The most recent thumbnail replaces the
prior temporary copy on the SD card. Missing photos leave the flight details
usable and show a high-contrast status message in the image frame. Temporary
lookup or download failures are retried once automatically.

![Aircraft detail screen](images/aircraft-screen.jpg)

![Aircraft detail screen when no photograph is available](images/aircraft-no-photo-screen.jpg)

## Bambuddy

Bambuddy is read-only. It calls the configured printer status endpoint and
shows:

- Printer name, connection state, and current state
- Active print name and completion percentage
- Remaining time and current/total layer
- Nozzle and bed temperatures with targets
- Printer Wi-Fi signal

Use an API key limited to status reading. The firmware does not expose printer
control, queue management, or job cancellation.

![Bambuddy printer status screen](images/bambuddy-screen.jpg)

## Systems

Systems combines local device health with simple connectivity checks. It shows
free heap, Wi-Fi signal, uptime, SD state, DNS resolution, internet reachability,
and response time.

Up to four custom monitors can be configured:

- **HTTP:** a plain HTTP request is healthy for status 200 through 399.
- **TCP:** a connection to the configured host and port is healthy.

Each monitor has its own enable switch. Disabled monitors are neither checked
nor displayed. The standalone design does not read CPU, disk, container, or
temperature data from another machine.

![Device and network systems screen](images/systems-screen.jpg)

## Calendar

Calendar is generated locally from the synchronized ESP32 clock. It starts the
week on Sunday, highlights today, and provides previous and next month buttons.
Opening the tile resets the view to the current month.

Calendar does not download a feed, store account credentials, or depend on
another service.

![Local monthly calendar screen](images/calendar-screen.jpg)

## On-device settings

The touch settings pages provide quick access to display orientation,
brightness, tile visibility, service summaries, network status, portal
address, and the generated bootstrap password. Text-heavy configuration is
kept in the browser portal rather than an on-screen keyboard.

![On-device settings menu](images/settings-screen.jpg)

The Firmware tile sits on the final row of the scrollable Settings page.

![Firmware tile on the scrollable Settings page](images/settings-firmware-screen.jpg)

Every page has a compact top bar. WiFi and SD badges use green for available
and red for unavailable. Four ascending bars beside WiFi show the current
signal level. The local time appears at the right after SNTP synchronizes.

Settings > System records the Wi-Fi channel and RSSI in dBm when the page
opens, alongside the portal address and device memory.

### Firmware updates

Settings > Firmware shows the installed firmware version and checks the latest
stable GitHub release on demand. When a newer release is available, the device
offers a two-tap confirmed installation. When the installed release is already
current, the same screen can reinstall it.

The firmware binary streams directly into the inactive OTA partition. GitHub's
certificate-validated Releases API supplies the release version, exact byte
count, download URL, and SHA-256 digest. The separately downloaded binary must
match that authenticated size and digest before the inactive partition is
activated. A failed, interrupted, or corrupt download leaves the running
firmware selected. Configuration, touch calibration, and SD-card files are not
part of the OTA image. After reboot, the ESP32 confirms the new image only
after display, storage, configuration, and background services finish starting.
The bootloader can roll back if startup fails before that point.

![Installed firmware and current GitHub release on the device](images/firmware-screen.jpg)

See [Over-the-air updates](OTA-UPDATES.md) for device installation, release
publication, post-update checks, and USB recovery.

## Browser portal

The portal edits ordinary settings and write-only credentials. It also includes
an SD-card file manager, firmware update controls, and a dedicated
startup-artwork upload. Sections collapse to keep the page manageable on
phones. Saves are validated, staged, and redirected back to the main page.

On first boot, the device opens the password-protected
`desktopdashboard-setup` network for initial configuration.

![Browser settings portal with collapsed sections](images/web-portal-overview.png)

## Reliability behavior

Network work runs on the other ESP32 core. A shared mutex prevents OTA from
overlapping the serialized live-data worker's TLS requests. LVGL rendering
remains in the main loop. SD access uses a separate mutex, preventing portal
operations from colliding with cached flight data or icon reads.

Failed services show a useful error on their page without stopping the rest of
the dashboard. Existing cached snapshots remain available while a new request
is queued.
