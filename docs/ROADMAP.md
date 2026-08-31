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

The standard artwork, SD override, validated portal upload, and dimension check
are complete. Configurable startup text and display duration remain possible
future additions.

## Longer-term ideas

- Configurable CA certificates for local HTTPS services
- Export and import of sanitized configuration
- Optional LVGL simulator for automated UI screenshots
- Accessibility review for color contrast and touch-target sizing
