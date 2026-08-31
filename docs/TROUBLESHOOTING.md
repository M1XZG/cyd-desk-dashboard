# Troubleshooting

## White, blank, or scrambled display

Confirm the board is an ILI9341-based 320x240 CYD and compare its pins with
[Hardware](HARDWARE.md). A different display revision needs a different
LovyanGFX panel definition.

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
429, the firmware uses MET Norway until the Open-Meteo limit has had time to
reset.

## Flights reports no aircraft

Increase the search radius, lower the minimum altitude, or try the alternative
free provider. A quiet area can legitimately return no traffic.

## Flights reports HTTP -1

A negative HTTP status means the connection failed before the provider returned
an HTTP response. Confirm the clock, DNS, and internet state, then allow the next
scheduled refresh to retry. Current firmware includes the ISRG Root X1 trust
anchor required by the certificate chain used by `adsb.lol`.

## Bambuddy is unavailable

Check host, port, printer ID, API base path, and the read-only key. The device
must be able to reach the Bambuddy host from its Wi-Fi network.

## A Systems monitor is missing

The monitor must be enabled and have both a name and host. Disabled or
incomplete monitors are skipped and omitted from the device page.

## Calendar shows the wrong day

Calendar uses the local UTC offset received with successful Weather data. Until
Weather loads, it falls back to UTC. Confirm the configured location and force
a Weather refresh by reopening the Weather page.

## Capture serial diagnostics

The included helper opens a port without intentionally toggling reset:

```powershell
py firmware/tools/capture_serial.py --port COM4 --duration 25
```

Serial output includes SD state, Wi-Fi state, heap, stack headroom, page name,
provider status, and portal diagnostics. Redact private IP addresses, Wi-Fi
names, locations, and service names before sharing a capture. Current firmware
does not print the bootstrap portal password.
