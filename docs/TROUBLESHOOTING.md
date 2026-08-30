# Troubleshooting

## White, blank, or scrambled display

Confirm the board is an ILI9341-based 320x240 CYD and compare its pins with
[Hardware](HARDWARE.md). A different display revision needs a different
LovyanGFX panel definition.

## Touch is offset or reversed

Re-run calibration after changing rotation. If calibration data is invalid,
clear the `touch` NVS namespace or hold the touch IRQ active during boot to
force calibration.

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
the device has a plausible time.

## Flights reports no aircraft

Increase the search radius, lower the minimum altitude, or try the alternative
free provider. A quiet area can legitimately return no traffic.

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
provider status, and portal diagnostics.
