# Artwork and SD Assets

The menu and weather-condition icons come from
[Lucide](https://lucide.dev/), an open-source icon set distributed under the
ISC licence. The original SVG files and a copy of the licence are stored in
`assets/icons/`.

| Tile | Source icon | Runtime SD filename |
| --- | --- | --- |
| Weather | `cloud-sun.svg` | `/dashboard/icons/Weather.bin` |
| Flights | `plane.svg` | `/dashboard/icons/Flights.bin` |
| Bambuddy | `printer.svg` | `/dashboard/icons/Bambuddy.bin` |
| Systems | `server.svg` | `/dashboard/icons/Systems.bin` |
| Calendar | `calendar-days.svg` | `/dashboard/icons/Calendar.bin` |
| Settings | `settings.svg` | `/dashboard/icons/Settings.bin` |
| Clear day | `sun.svg` | `/dashboard/icons/Sun.bin` |
| Clear night | `moon.svg` | `/dashboard/icons/Moon.bin` |
| Partly cloudy night | `cloud-moon.svg` | `/dashboard/icons/CloudMoon.bin` |
| Overcast | `cloud.svg` | `/dashboard/icons/Cloud.bin` |
| Fog | `cloud-fog.svg` | `/dashboard/icons/Fog.bin` |
| Drizzle and rain | `cloud-rain.svg` | `/dashboard/icons/Rain.bin` |
| Snow | `cloud-snow.svg` | `/dashboard/icons/Snow.bin` |
| Thunderstorm | `cloud-lightning.svg` | `/dashboard/icons/Thunderstorm.bin` |

The PNG versions in `sd-card/dashboard/assets/icons/` are 48x48 with
transparency and remain the editable raster sources. The runtime files in
`sd-card/dashboard/icons/` use LVGL 8's 4-bit alpha format. Each 48x48 icon is
1,156 bytes including its four-byte header, is coloured by the tile style, and
can be read a line at a time without enabling the PNG decoder.

The firmware carries the same compact runtime assets as defaults. At boot it
creates any missing files in `/dashboard/icons/` but does not overwrite
existing files, so users can replace individual icons on the SD card. A missing
or invalid icon falls back to the tile's initial letter. Replacement files must
use the same 48x48 LVGL 4-bit alpha format and 1,156-byte file size.

## Startup artwork

`assets/startup-logo.svg` is the editable source for the standard startup
screen. `assets/startup-logo.png` is the lossless 320x240 master, while
`sd-card/dashboard/startup.jpg` is the compact runtime copy.

The JPEG must be exactly 320x240 pixels, use RGB color, and remain below 200 KB.
The firmware displays an embedded copy when the SD file is missing or invalid.
Regenerate the SD image and embedded fallback with:

```bash
python3 tools/build_startup.py
```

Regenerate the runtime files and embedded defaults from the source PNGs with:

```bash
python3 tools/build_icons.py
```

The mockup is 320x240, matching the physical landscape resolution. It is a
layout guide rather than a final theme.

## Board reference images

`docs/images/board-front.jpg`, `docs/images/board-back.jpg`, and
`docs/images/board-pinout.jpg` came from the
[linked ESP32-2432S028 Amazon listing](https://link.amazon/B0e12EwEV), which is
an affiliate link. The repository owner supplied the source files and confirmed
permission to redistribute them. These images are not covered by the
repository's MIT licence. Repository copies have been resized and stripped of
embedded camera or source metadata.
