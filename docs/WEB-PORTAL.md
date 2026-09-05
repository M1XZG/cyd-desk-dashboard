# Browser portal

## First-run setup

An unconfigured dashboard creates the `desktopdashboard-setup` Wi-Fi network
with password `deskdashboard`. Open `http://192.168.4.1/` and enter the target
Wi-Fi credentials, a new portal password, and the POSIX timezone. The setup
network is available only while Wi-Fi has not been configured and stops when
station mode connects.

## Access

The portal starts after Wi-Fi connects. Open the IP shown under
**Settings > System** or `http://desk-dashboard.local/`.

Authentication uses HTTP Basic Auth:

| Field | Value |
| --- | --- |
| Username | `admin` |
| Password | Value from `connections.json`, or the bootstrap password shown on the device |

The portal is intended for a trusted LAN. It does not provide HTTPS.

![Browser settings portal overview](images/web-portal-overview.png)

## Settings sections

### Display

Set brightness from 5 to 100 percent and choose normal or 180-degree rotation.
Changing rotation schedules a restart and requires touch recalibration.
Night mode controls its start and end times and the inactivity period before
the backlight switches off. It is enabled by default from 23:00 to 08:00 with
a five-minute timeout.

![Display settings section](images/portal-display.png)

### Location and units

Enter a town or postcode and choose 12/24-hour time, date format, temperature,
wind, pressure, precipitation units, and a POSIX timezone. The timezone drives
the top-bar clock and Calendar independently of Weather.

![Location and format settings section](images/portal-location-formats.png)

### Flights

Choose airplanes.live, adsb.lol, or Flightradar24. Set the radius, maximum
aircraft, minimum altitude, refresh interval, and optional provider token.

![Flights settings section](images/portal-flights.png)

### Bambuddy

Set the HTTP host, port, API base path, printer ID, and read-only key. Blank
credential fields preserve the current value. The existing key is never sent
back to the browser. The same read-status key covers AMS data and the
short-lived token used for an on-demand camera snapshot.

![Bambuddy settings section](images/portal-bambuddy.png)

### Systems

Set the refresh interval and configure up to four HTTP or TCP checks. Each
monitor has an enable checkbox, name, host, port, and HTTP path.

![Systems monitoring settings section](images/portal-systems.png)

### Home tiles

Show or hide Weather, Flights, Bambuddy, Systems, and Calendar. Settings cannot
be hidden.

![Home tile visibility settings](images/portal-home-tiles.png)

### Wi-Fi and portal security

Set the Wi-Fi SSID and password, then change the portal password. New portal
passwords must contain at least eight characters.

![Wi-Fi network settings](images/portal-wifi.png)

![Portal password settings](images/portal-security.png)

### Startup artwork

Upload a 320x240 baseline JPEG no larger than 200 KB. Progressive JPEGs are not
supported. The firmware stages the file, checks its JPEG dimensions and size,
then atomically replaces
`/dashboard/startup.jpg`. Restart the dashboard to see the new image.

### Firmware updates

The firmware section shows the installed version and the newest stable GitHub
release. **Check for updates** fetches a small release manifest. A newer release
can then be installed after browser confirmation. If the versions match, the
same control reinstalls the current release.

The browser-assisted updater avoids GitHub TLS on the ESP32. Select **Prepare
browser update**, download the release linked by the page, then choose that file
under **Verify and install**. The browser checks the file against the SHA-256
digest returned by GitHub before upload, and the ESP32 independently checks the
same digest while streaming the firmware into the inactive slot.

During installation, the portal reports download progress and waits for the
dashboard to restart. The firmware size and SHA-256 digest are verified before
the inactive OTA partition is selected for boot.

![Firmware update section after installing v1.1.3](images/web-portal-firmware.png)

`Latest release: not checked` is the normal initial state. Select **Check for
updates** to contact GitHub. Keep the device powered during installation and
allow about 20 seconds for the portal to return after restart. See
[Over-the-air updates](OTA-UPDATES.md) for the complete user and release
process.

## Save behavior

The form includes a per-boot token. The firmware validates numeric ranges and
known option values before writing. It creates stage and backup files, replaces
both JSON documents as one operation, and recovers interrupted saves during the
next boot.

A successful save redirects to the portal root rather than leaving the browser
on `/save`. Wi-Fi or rotation changes schedule a restart.

The manual restart page waits 15 seconds, then returns the browser to the portal
root after the device has had time to boot.

## SD file manager

Select **Browse SD card files** to:

- Browse directories
- Download a file
- Upload a file up to 8 MB
- Create a directory
- Delete one file or an empty directory

Uploads do not silently overwrite existing files. Paths containing traversal
components are rejected. Recursive deletion is intentionally unavailable.

![SD-card file manager](images/web-file-manager.png)

## Health endpoint

`/health` returns a small unauthenticated status response suitable for checking
whether the portal task is alive. It does not expose credentials.
