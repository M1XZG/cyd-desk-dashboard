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

Copy the repository's `sd-card/dashboard` folder to the root of the card.
Rename:

```text
dashboard/config.example.json      -> dashboard/config.json
dashboard/connections.example.json -> dashboard/connections.json
```

Edit `connections.json` with your Wi-Fi details. The SD card is not encrypted,
so use read-only service keys and keep the card physically secure.

The firmware can boot without either live JSON file. It creates versioned
examples when they are missing, but Wi-Fi and the portal remain unavailable
until credentials are supplied.

`dashboard/startup.jpg` is optional. The supplied file matches the embedded
default. Replace it with another 320x240 RGB JPEG under 200 KB to customize the
startup artwork without rebuilding the firmware.

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
calibration and the generated portal password. Confirm that the partition
layout matches `min_spiffs.csv` before using this method.

## First boot

The first boot opens touch calibration if no valid calibration is stored.
Touch each target carefully with a stylus or fingernail. Calibration is kept in
ESP32 NVS.

If the display is upside down, open **Settings > Display** and select
**Rotate 180**. Rotation changes require recalibration because the touch axes
change with the screen orientation.

## Open the browser portal

After Wi-Fi connects, open the IP shown under **Settings > System** or try:

```text
http://desk-dashboard.local/
```

The username is `admin`. If no portal password exists in `connections.json`,
the device generates one and shows it on the physical System page. Change it
through the portal after signing in.

## Configure live services

Weather needs only a town or postcode. Flights works without a key when using
airplanes.live or adsb.lol. Bambuddy requires its host, printer ID, and a
read-only API key. Systems monitors are optional.

See [Web portal](WEB-PORTAL.md) for each field and
[Features](FEATURES.md) for provider behavior.
