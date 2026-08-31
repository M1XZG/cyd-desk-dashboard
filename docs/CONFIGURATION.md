# Configuration

The dashboard stores ordinary settings separately from credentials:

| File | Purpose |
| --- | --- |
| `/dashboard/config.json` | Display, location, units, services, refresh intervals, and tile visibility |
| `/dashboard/connections.json` | Wi-Fi, API keys, provider tokens, and portal password |
| `/dashboard/startup.jpg` | Optional 320x240 startup artwork |

Start from the `.example.json` files in `sd-card/dashboard`. The browser portal
is the preferred editor after the first Wi-Fi connection.

## `config.json`

### Display

`display.rotation` accepts `1` for normal landscape and `3` for 180-degree
rotation. `brightness_percent` is constrained to 5-100.

### Locale and weather

Supported portal values include:

| Setting | Values |
| --- | --- |
| Time format | `12h`, `24h` |
| Date format | `yyyy-dd-mm`, `dd-mm-yyyy`, `mm-dd-yyyy`, `yyyy-mm-dd` |
| POSIX timezone | A POSIX TZ rule, up to 80 printable characters |
| Temperature | `celsius`, `fahrenheit` |
| Wind | `kmh`, `mph` |
| Pressure | `hpa`, `inhg` |
| Precipitation | `mm`, `in` |

`location.search` accepts a place name or postcode. `locale.posix_timezone`
sets the device clock independently of Weather and includes daylight-saving
rules when the supplied POSIX string defines them.

The time preference controls the top-bar clock. Date and precipitation
preferences are preserved but do not yet change the current pages.

### Flights

| Field | Accepted range |
| --- | --- |
| `provider` | `airplanes.live`, `adsb.lol`, `flightradar24` |
| `radius_km` | 10-250 |
| `maximum_aircraft` | 1-20 |
| `minimum_altitude_ft` | 0-60,000 |
| `refresh_seconds.flights` | 15-900 |

### Bambuddy

Configure `host`, `port`, `api_base_path`, and `printer_id`. The current
firmware supports HTTP on a trusted LAN; HTTPS is reserved for a future release
with configurable certificate authorities. The default API path is `/api/v1`.

### Systems monitors

Each object in `services.systems.monitors` supports:

| Field | Meaning |
| --- | --- |
| `enabled` | Query and display this monitor |
| `name` | Label shown on the device, up to 24 characters |
| `type` | `http` or `tcp` |
| `host` | Hostname or IP address |
| `port` | 1-65,535 |
| `path` | HTTP path beginning with `/` |

At most four monitors are used. `refresh_seconds.systems` is constrained to
15-900 seconds.

### Tile visibility

Set entries under `tile_visibility` to `true` or `false`. Settings always
remains visible.

## `connections.json`

```json
{
  "version": 1,
  "wifi": {
    "ssid": "YOUR_WIFI_NETWORK",
    "password": "YOUR_WIFI_PASSWORD"
  },
  "services": {
    "bambuddy_readonly_api_key": "",
    "flights_api_token": ""
  },
  "device": {
    "settings_portal_password": ""
  }
}
```

The SD card is readable outside the device. Never store administrator keys,
Google account tokens, printer-control credentials, or a reused account
password in this file.

## Generated and recovery files

The firmware creates versioned examples if they are absent. Portal saves use
temporary stage and backup files. A boot-time recovery pass restores a
consistent pair after interrupted power or a failed rename.

Live `config.json` and `connections.json` are ignored by Git. Do not force-add
them to a repository.

The complete distributable examples are
[`config.example.json`](../sd-card/dashboard/config.example.json) and
[`connections.example.json`](../sd-card/dashboard/connections.example.json).

## Startup artwork

The startup file must be a 320x240 baseline RGB JPEG no larger than 200 KB.
Progressive JPEG encoding is not supported by the display decoder. Keep the
bottom 32 pixels visually quiet because the firmware draws its startup status
there. The browser portal validates the JPEG dimensions and size before
replacing the active file. If the file is missing, invalid, oversized, or
cannot be decoded, the embedded standard artwork is shown.
