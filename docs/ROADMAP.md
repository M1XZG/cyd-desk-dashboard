# Roadmap

## OTA firmware updates

Add a firmware section to Settings and the browser portal with:

- An enable switch for update checks
- The installed firmware version
- The newest available release version
- A manual **Check for updates** action
- A confirmed OTA installation flow with progress and failure recovery

The implementation must verify downloaded firmware, preserve configuration and
touch calibration, and avoid leaving the device unbootable after interrupted
power.

## Home tile ordering

Allow users to change the order of the five data tiles while keeping Settings
available. The browser portal is the likely first editor. A later touch
interface could add drag-and-drop or move-left/move-right controls.

## Personalized startup screen

The standard startup artwork and SD-hosted `/dashboard/startup.jpg` override
are complete. Remaining work:

- Add a portal upload and preview control dedicated to startup artwork
- Add configurable startup text
- Allow the display duration to be configured
- Validate image dimensions before replacing the active file

## Longer-term ideas

- Configurable CA certificates for local HTTPS services
- Export and import of sanitized configuration
- Optional LVGL simulator for automated UI screenshots
- Signed release binaries and checksum publication
- Accessibility review for color contrast and touch-target sizing
