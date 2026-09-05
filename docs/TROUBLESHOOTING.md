# Troubleshooting

## White, blank, or scrambled display

Confirm the board is an ILI9341-based 320x240 CYD and compare its pins with
[Hardware](HARDWARE.md). A different display revision needs a different
LovyanGFX panel definition.

## Screen switches off overnight

Night mode switches off only the backlight; the dashboard continues updating.
Touch the screen once to wake it. That wake touch is consumed, so touch the
intended control separately after the display lights. The default schedule is
23:00-08:00 with a five-minute inactivity timeout. Change or disable it in the
Display section of the browser portal.

## Touch is offset or reversed

Re-run calibration after changing rotation. To force calibration, press and
hold the touchscreen while powering on the device, then release it when the
calibration targets appear. If that does not work, erase the device NVS with
`pio run -e cyd -t erase` and reflash; this also removes saved rotation and the
generated portal password.

## Device returns to the home screen

Use a stable power supply and inspect serial output at 115200 baud. Navigation
is deferred by design, so repeated resets usually indicate power, memory, or a
hardware mismatch rather than a normal page transition.

Firmware v1.1.5 fixes a `LoadProhibited` crash in the LVGL SD icon reader that
could occur during repeated live-page redraws. Update if a v1.1.4 device
reboots while left on Bambuddy or another icon-bearing screen.

## SD card shows unavailable

Use FAT32 and place the `dashboard` folder at the card root. Reinsert the card
with power removed. Confirm the board revision uses the documented SD pins.

## Wi-Fi does not connect

The ESP32 supports 2.4 GHz Wi-Fi. Check the live
`/dashboard/connections.json`, SSID spelling, password, and signal strength.
The example file alone is not loaded.

## Portal does not open

Open the IP shown on **Settings > System**. mDNS names do not resolve on every
network, so use the numeric address if `desk-dashboard.local` fails.

## Portal password is unknown

When no password is configured, the device generates a bootstrap password and
shows it on the physical System page. Existing passwords are never displayed
in the browser.

## Weather is unavailable

Set a place or postcode, confirm DNS and internet access on the Systems page,
and wait for the clock to synchronize. TLS requests cannot be validated before
the device has a plausible time. The dashboard caches resolved coordinates and
limits weather refreshes to once every ten minutes. If Open-Meteo returns HTTP
429 or its forecast connection fails, the firmware uses MET Norway. Firmware
`v1.1.4` also trusts Open-Meteo's current Let's Encrypt `Root YR` chain directly
so the no-PSRAM board does not have to validate its larger bridge certificate.

## Flights reports no aircraft

Increase the search radius, lower the minimum altitude, or try the alternative
free provider. A quiet area can legitimately return no traffic.

## Flights reports HTTP -1

A negative HTTP status means the connection failed before the provider returned
an HTTP response. Confirm the clock, DNS, and internet state, then allow the next
scheduled refresh to retry. Firmware `v1.1.5` limits failed retries to once per
minute and supports the newer Let's Encrypt YR chain used by `adsb.lol`.

Arduino ESP32 2.0.17 cannot currently validate that chain without consuming
most of the no-PSRAM board's largest free heap block. Firmware `v1.1.5` uses
the providers' direct HTTP endpoints for bounded public aircraft and weather
data; neither endpoint receives credentials. Aircraft-cache parsing yields to
the scheduler so a large response cannot starve the Core 0 watchdog. If the
default `airplanes.live` TLS connection fails before an HTTP response arrives,
the firmware automatically retries the compatible `adsb.lol` HTTP endpoint.
The `adsb.lol` connection resolves its canonical origin separately to avoid an
ESP32 DNS-client failure with the provider's public CNAME response.

## Bambuddy is unavailable

Check host, port, printer ID, API base path, and the read-only key. The device
must be able to reach the Bambuddy host from its Wi-Fi network.

## AMS information is missing

Confirm Bambuddy shows the AMS and its slots for the selected printer. The
dashboard reads `ams`, `ams_exists`, and `tray_now` from the normal status
response. Empty or unidentified third-party spools may appear as an unknown
spool when the printer reports presence without RFID details.

## The Bambuddy camera does not load

Confirm the printer camera works in Bambuddy itself. The dashboard uses the
read-only key to request a short-lived camera token, then downloads one JPEG
snapshot. HTTP 401 or 403 means the key cannot use Bambuddy's camera-view
route. HTTP 503 normally means Bambuddy could not capture a frame from the
printer.

The snapshot requires a mounted SD card, must be a baseline JPEG, and cannot
exceed 512 KB. The previous image is replaced only after a complete,
JPEG-validated download.

## A Systems monitor is missing

The monitor must be enabled and have both a name and host. Disabled or
incomplete monitors are skipped and omitted from the device page.

## Firmware update says not checked

This is the normal state before a manual check. Open **Settings > Firmware** or
the browser portal's **Firmware updates** section and select **Check for
updates**.

## Firmware update check fails

Confirm Wi-Fi, DNS, internet access, and synchronized time on the Systems page.
Wait for Weather or Flights activity to finish, then retry. OTA shares the
device's single TLS network slot with live-data requests because the original
CYD has no PSRAM.

An update-check error does not change the installed firmware. If GitHub returns
HTTP `-1`, restart the dashboard, wait for the clock to appear in the top bar,
and check again. Firmware v1.3.1 extends GitHub TLS handshakes from eight to
twenty seconds and reports a failed HTTPS connection without incorrectly
describing every TLS timeout as a refused connection.

If direct checks remain unreliable, open the browser portal and use
**Browser-assisted update**. The browser handles GitHub HTTPS and verifies the
official release digest; the dashboard receives only the validated firmware
stream and verifies SHA-256 again before activation.

## Firmware installation fails or stops

Leave the device powered until it reports an error or restarts. A short,
corrupt, or interrupted download is rejected before the new partition is
activated, so the existing firmware remains bootable. Restart and retry from a
stable Wi-Fi connection.

If the new image begins booting but cannot finish startup, the
rollback-enabled bootloader can select the previous OTA slot. Use a full USB
flash only if the dashboard no longer reaches the interface or the release
changes the bootloader or partition table. The complete procedure is in
[Over-the-air updates](OTA-UPDATES.md).

## Calendar shows the wrong day

Calendar uses the local UTC offset received with successful Weather data. Until
Weather loads, it falls back to UTC. Confirm the configured location and force
a Weather refresh by reopening the Weather page.

## Capture serial diagnostics

The included helper opens a port without intentionally toggling reset:

```powershell
py firmware/tools/capture_serial.py --duration 25
```

The helper identifies a connected CH340 adapter automatically. If more than
one suitable serial port is present, specify the dashboard with `--port COMx`.

Serial output includes SD state, Wi-Fi state, heap, stack headroom, page name,
provider status, and portal diagnostics. Redact private IP addresses, Wi-Fi
names, locations, and service names before sharing a capture. Current firmware
does not print the bootstrap portal password.
