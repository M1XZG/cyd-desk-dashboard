# Releases

Tagged versions are published through GitHub Releases. Each release page also
offers GitHub's automatically generated source archives. Building from source
uses the pinned PlatformIO platform, Arduino framework, libraries, and Python
tools recorded in the repository. CI release builds use a fixed
`SOURCE_DATE_EPOCH` to remove clock-driven build variance. PlatformIO builds
made on another host can still differ at the byte level, so the `SHA256SUMS`
file attached to each release is the authoritative digest for its downloads.

## Firmware files

`firmware.bin` is the application image. Use it for an application-only update
when the board already has a compatible bootloader and partition table. With
esptool, flash it at address `0x10000`.

A clean or recovered board needs the full set of images. Flash
`bootloader.bin` at `0x1000`, `partitions.bin` at `0x8000`, `boot_app0.bin` at
`0xe000`, and `firmware.bin` at `0x10000`. These offsets are for the release's
CYD build and its `min_spiffs.csv` partition layout. Do not substitute a
partition image from a different build.

`SBOM.spdx.json` lists the pinned build platform, framework, firmware libraries,
and repository Python tooling in SPDX 2.3 JSON format. `SHA256SUMS` contains
the digest of every binary and the SBOM. Verify downloads from the directory
that contains them:

```sh
sha256sum --check SHA256SUMS
```

For public tagged releases, GitHub also records a signed build-provenance
attestation for the files named in `SHA256SUMS`. With a current GitHub CLI,
verify an artifact against this repository:

```sh
gh attestation verify firmware.bin --repo M1XZG/cyd-desk-dashboard
```

The checksum detects a damaged or substituted download. The attestation ties
the artifact digest to the GitHub Actions build that produced the tagged
release.
