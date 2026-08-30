CYD Desk Dashboard SD Card
==========================

Copy this dashboard folder to the root of a FAT32-formatted microSD card.

Rename config.example.json to config.json for ordinary settings. Rename
connections.example.json to connections.json for Wi-Fi and service
credentials. The browser portal is the preferred editor after the first Wi-Fi
connection.

The icons folder contains the 48x48 LVGL alpha assets used by the device.
Missing files are recreated from built-in defaults. Existing replacements are
never overwritten.

startup.jpg is the optional startup artwork. It must be a 320x240 RGB JPEG no
larger than 200 KB. The firmware uses an embedded fallback if it is absent or
invalid.

The SD card is not encrypted. Anyone who removes it can read connections.json.
Use dedicated read-only service keys and a portal password that is not reused
elsewhere.

The firmware creates versioned example files when they are absent. It does not
overwrite live configuration files.
