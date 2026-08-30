# Contributing

## Development setup

Install Python 3 and PlatformIO Core, then build from `firmware`:

```bash
pio run -e cyd
```

Keep dependencies pinned unless a change requires an upgrade. Do not commit
`.pio`, virtual environments, live SD configuration, credentials, flight
caches, or serial captures containing private network details.

## Code changes

Keep LVGL object creation and mutation in the main loop. Touch callbacks should
request deferred navigation rather than cleaning the active screen directly.
Network operations belong in the serialized live-data worker. Every SD access
must use the shared storage mutex.

The target has no PSRAM. Bound response sizes, avoid large automatic variables,
and prefer streaming or fixed-size result structures.

## Validation

Build the `cyd` environment and test affected screens on physical hardware.
Check serial output for resets, watchdog warnings, shrinking heap, and low stack
headroom. Portal changes should be tested on both a desktop and a phone-sized
browser.

## Pull requests

Describe the user-visible change, hardware tested, configuration migration, and
screens affected. Include photographs for visible UI changes when possible.
Never include real credentials or private service addresses.
