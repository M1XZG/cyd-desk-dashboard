# Screenshots and photographs

The current firmware does not expose the display framebuffer, and its LVGL
pages depend directly on ESP32 hardware, SD storage, and live service state.
There is no ready-made emulator that can reproduce the screens accurately
without a separate simulator port.

The browser portal can be captured in a normal browser, but the most faithful
documentation for the device UI is a straight-on photograph of each physical
screen. Placeholders are already linked throughout the documentation.

## Needed images

| File to replace | Screen |
| --- | --- |
| `docs/images/home-screen.svg` | Home tile grid |
| `docs/images/weather-screen.svg` | Weather |
| `docs/images/flights-screen.svg` | Flights radar |
| `docs/images/aircraft-screen.svg` | Aircraft details |
| `docs/images/bambuddy-screen.svg` | Bambuddy |
| `docs/images/systems-screen.svg` | Systems |
| `docs/images/calendar-screen.svg` | Calendar |
| `docs/images/settings-screen.svg` | On-device Settings |
| `docs/images/web-portal.svg` | Browser settings portal |

Use landscape images, crop closely around the screen or browser viewport, and
hide private IP addresses, Wi-Fi names, API keys, printer names, and personal
locations. PNG or WebP is preferred for the final images. After adding a real
image, update the matching Markdown link from `.svg` to the new extension.
