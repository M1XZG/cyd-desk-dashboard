# Getting started

## What you need

- An ESP32-2432S028 CYD with the 2.8-inch 320x240 ILI9341 display and XPT2046
  resistive touch controller
- A FAT32-formatted microSD card
- A data-capable USB cable
- Python 3 and [PlatformIO Core](https://platformio.org/install/cli)
- A 2.4 GHz Wi-Fi network for live data and the browser portal

The tested board has 4 MB flash and no PSRAM. Other CYD revisions may route the
display, touch controller, backlight, or SD slot differently.

## Prepare the SD card

Copy the repository's `sd-card/dashboard` folder to the root of the card. You
can rename the example files for manual configuration:

```text
dashboard/config.example.json      -> dashboard/config.json
dashboard/connections.example.json -> dashboard/connections.json
```

Manual editing is optional. On a new installation, the dashboard starts its
protected setup network when live Wi-Fi credentials are absent.

The firmware can boot without either live JSON file and creates them after
browser setup. The SD card is not encrypted, so use read-only service keys and
keep the card physically secure.

`dashboard/startup.jpg` is optional. The supplied file matches the embedded
default. Replace it with another 320x240 baseline RGB JPEG under 200 KB to
customize the startup artwork without rebuilding the firmware.

## Build

From the repository root:

```bash
cd firmware
pio run -e cyd
```

The application binary is written to:

```text
firmware/.pio/build/cyd/firmware.bin
```

Dependencies are pinned in `firmware/platformio.ini`.

## Flash

For a normal PlatformIO upload:

```bash
cd firmware
pio run -e cyd -t upload --upload-port /dev/ttyUSB0
```

On Windows, replace the port with the board's COM port:

```powershell
pio run -e cyd -t upload --upload-port COM4
```

Advanced users updating an existing installation can write only the
application partition at `0x10000`. This preserves NVS values such as touch
calibration and the generated portal password. Only use this method for a
device already flashed with this project's `cyd` environment, which selects
PlatformIO's `min_spiffs.csv` partition layout in `firmware/platformio.ini`.

## First boot

The first boot opens touch calibration if no valid calibration is stored.
Touch each target carefully with a stylus or fingernail. Calibration is kept in
ESP32 NVS.

If the display is upside down, open **Settings > Display** and select
**Rotate 180**. Rotation changes require recalibration because the touch axes
change with the screen orientation.

## Connect the dashboard to Wi-Fi

When Wi-Fi has not been configured, connect a phone or computer to:

| Field | Value |
| --- | --- |
| Network | `desktopdashboard-setup` |
| Password | `deskdashboard` |
| Setup address | `http://192.168.4.1/` |

Most devices open the setup page automatically. Enter the destination Wi-Fi
details, create a portal password of at least eight characters, and confirm the
POSIX timezone. The dashboard saves both JSON files and restarts.

The default timezone is the United Kingdom rule
`GMT0BST,M3.5.0/1,M10.5.0`. Replace it during setup when the dashboard will be
used elsewhere.

If configured Wi-Fi later becomes unavailable, correct `connections.json` on
the SD card. The public setup password is deliberately limited to first-run
devices and cannot overwrite an existing configuration.

## Open the browser portal

After Wi-Fi connects, open the IP shown under **Settings > System** or try:

```text
http://desk-dashboard.local/
```

The username is `admin`. If no portal password exists in `connections.json`,
the device generates one and shows it on the physical System page. Change it
through the portal after signing in.

SNTP starts as soon as Wi-Fi connects. The top bar shows the local time and
uses green or red WiFi and SD badges to show their current state. Four small
bars show Wi-Fi signal strength. Settings > System shows the channel and RSSI
reading in dBm.

## Configure live services

Weather needs only a town or postcode. Flights works without a key when using
airplanes.live or adsb.lol. Bambuddy requires its host, printer ID, and a
read-only API key. Systems monitors are optional.

See [Web portal](WEB-PORTAL.md) for each field and
[Features](FEATURES.md) for provider behavior.

## Install later firmware releases

Open **Settings > Firmware** on the device or **Firmware updates** in the
browser portal. Select **Check for updates**, review the installed and latest
versions, then confirm the installation. If both versions match, the install
control becomes **Reinstall current release**.

Keep the dashboard powered during installation. OTA replaces only
`firmware.bin` and preserves NVS calibration, configuration, and SD-card
content. A release that changes the bootloader or partition table still
requires the full USB flashing procedure.
