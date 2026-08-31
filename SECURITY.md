# Security

## Supported version

Security fixes currently target the latest commit on the default branch.

## Reporting

Use GitHub's private vulnerability reporting feature when it is enabled for the
repository. Do not open a public issue containing credentials, private network
details, or a working exploit.

If private reporting is unavailable, open a public issue containing no
vulnerability details and ask the maintainer to provide a private contact
route.

## Deployment model

The dashboard is designed for a trusted home or office LAN. The browser portal
uses HTTP Basic Auth over unencrypted HTTP. Anyone who can observe that network
traffic may recover the portal credentials.

Do not expose the portal to the public internet or use it on an untrusted or
shared Wi-Fi network. Configure secrets from a trusted wired or private
wireless network.

The microSD card is also unencrypted. Anyone with physical access can read
Wi-Fi credentials and service keys from `connections.json`.

Use a dedicated portal password and least-privileged, read-only API keys. Do not
store administrator credentials, printer-control keys, account refresh tokens,
or passwords reused elsewhere.

## Built-in protections

- Credentials are not inserted into portal HTML
- Blank credential fields preserve stored values
- State-changing forms require a per-boot token
- Configuration saves are validated and staged
- SD file-manager paths reject traversal
- Uploads do not silently overwrite files
- Recursive deletion is unavailable
- Public HTTPS providers use embedded CA roots
