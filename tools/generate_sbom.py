#!/usr/bin/env python3

import argparse
import configparser
import hashlib
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PLATFORMIO_INI = ROOT / "firmware/platformio.ini"
REQUIREMENTS = ROOT / "requirements-tools.txt"
DEFAULT_OUTPUT = ROOT / "SBOM.spdx.json"
CREATED = "2026-08-31T00:00:00Z"
REPOSITORY_URL = "https://github.com/M1XZG/cyd-desk-dashboard"

FRAMEWORK_VERSIONS = {
    ("espressif32", "6.12.0", "arduino"): {
        "name": "Arduino ESP32",
        "version": "2.0.17",
        "download": "https://github.com/espressif/arduino-esp32/releases/tag/2.0.17",
        "purl": "pkg:github/espressif/arduino-esp32@2.0.17",
    }
}

LIBRARY_METADATA = {
    ("lovyan03", "LovyanGFX"): {
        "download": "https://github.com/lovyan03/LovyanGFX/releases/tag/1.2.28",
        "purl": "pkg:github/lovyan03/LovyanGFX@1.2.28",
    },
    ("lvgl", "lvgl"): {
        "download": "https://github.com/lvgl/lvgl/releases/tag/v8.4.0",
        "purl": "pkg:github/lvgl/lvgl@v8.4.0",
    },
    ("bblanchon", "ArduinoJson"): {
        "download": "https://github.com/bblanchon/ArduinoJson/releases/tag/v7.4.3",
        "purl": "pkg:github/bblanchon/ArduinoJson@v7.4.3",
    },
}

PYTHON_PURPOSES = {
    "pillow": "LIBRARY",
    "platformio": "APPLICATION",
    "pyserial": "LIBRARY",
}


def spdx_id(name: str) -> str:
    return "SPDXRef-" + re.sub(r"[^A-Za-z0-9.-]", "-", name)


def package(
    name: str,
    version: str,
    purpose: str,
    download: str,
    purl: str | None = None,
    declared_license: str = "NOASSERTION",
) -> dict[str, object]:
    result: dict[str, object] = {
        "SPDXID": spdx_id(f"{name}-{version}"),
        "name": name,
        "versionInfo": version,
        "downloadLocation": download,
        "filesAnalyzed": False,
        "licenseConcluded": "NOASSERTION",
        "licenseDeclared": declared_license,
        "copyrightText": "NOASSERTION",
        "primaryPackagePurpose": purpose,
    }
    if purl:
        result["externalRefs"] = [
            {
                "referenceCategory": "PACKAGE-MANAGER",
                "referenceType": "purl",
                "referenceLocator": purl,
            }
        ]
    return result


def platformio_dependencies() -> list[dict[str, object]]:
    config = configparser.ConfigParser(interpolation=None)
    config.read(PLATFORMIO_INI, encoding="utf-8")
    environment = config["env:cyd"]

    platform_match = re.fullmatch(
        r"(?:platformio/)?([A-Za-z0-9_.-]+)@([A-Za-z0-9_.-]+)",
        environment["platform"].strip(),
    )
    if platform_match is None:
        raise ValueError("env:cyd platform must be pinned with @version")
    platform_name, platform_version = platform_match.groups()
    framework_name = environment["framework"].strip()

    framework = FRAMEWORK_VERSIONS.get(
        (platform_name, platform_version, framework_name)
    )
    if framework is None:
        raise ValueError(
            "record the framework version for the pinned platform in "
            "FRAMEWORK_VERSIONS"
        )

    dependencies = [
        package(
            f"PlatformIO {platform_name} platform",
            platform_version,
            "FRAMEWORK",
            f"https://registry.platformio.org/platforms/platformio/"
            f"{platform_name}/version/{platform_version}",
            f"pkg:generic/platformio-{platform_name}@{platform_version}",
        ),
        package(
            str(framework["name"]),
            str(framework["version"]),
            "FRAMEWORK",
            str(framework["download"]),
            str(framework["purl"]),
        ),
    ]

    for line in environment["lib_deps"].splitlines():
        coordinate = line.strip()
        if not coordinate:
            continue
        match = re.fullmatch(r"([^/\s]+)/([^@\s]+)@([A-Za-z0-9_.-]+)", coordinate)
        if match is None:
            raise ValueError(f"library dependency is not pinned: {coordinate}")
        owner, name, version = match.groups()
        metadata = LIBRARY_METADATA.get((owner, name))
        if metadata is None:
            raise ValueError(f"record library metadata for {owner}/{name}")
        expected_suffix = f"@v{version}"
        if not str(metadata["purl"]).endswith(expected_suffix) and not str(
            metadata["purl"]
        ).endswith(f"@{version}"):
            raise ValueError(f"library metadata version differs for {owner}/{name}")
        dependencies.append(
            package(
                name,
                version,
                "LIBRARY",
                str(metadata["download"]),
                str(metadata["purl"]),
            )
        )
    return dependencies


def python_dependencies() -> list[dict[str, object]]:
    dependencies = []
    for raw_line in REQUIREMENTS.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        match = re.fullmatch(r"([A-Za-z0-9_.-]+)==([A-Za-z0-9_.-]+)", line)
        if match is None:
            raise ValueError(f"Python requirement is not pinned: {line}")
        name, version = match.groups()
        normalized_name = re.sub(r"[-_.]+", "-", name).lower()
        purpose = PYTHON_PURPOSES.get(normalized_name)
        if purpose is None:
            raise ValueError(f"record the purpose of Python tool {name}")
        dependencies.append(
            package(
                name,
                version,
                purpose,
                f"https://pypi.org/project/{name}/{version}/",
                f"pkg:pypi/{normalized_name}@{version}",
            )
        )
    return dependencies


def generate() -> dict[str, object]:
    root_package = package(
        "CYD Desk Dashboard firmware",
        "NOASSERTION",
        "FIRMWARE",
        REPOSITORY_URL,
        declared_license="MIT",
    )
    dependencies = sorted(
        platformio_dependencies() + python_dependencies(),
        key=lambda item: (str(item["name"]).lower(), str(item["versionInfo"])),
    )
    namespace_input = json.dumps(dependencies, sort_keys=True).encode("utf-8")
    namespace_hash = hashlib.sha256(namespace_input).hexdigest()

    return {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": "CYD Desk Dashboard firmware SBOM",
        "documentNamespace": (
            f"{REPOSITORY_URL}/sbom/spdx-2.3/{namespace_hash}"
        ),
        "creationInfo": {
            "created": CREATED,
            "creators": ["Tool: tools/generate_sbom.py"],
            "licenseListVersion": "3.26",
        },
        "documentDescribes": [root_package["SPDXID"]],
        "packages": [root_package, *dependencies],
        "relationships": [
            {
                "spdxElementId": "SPDXRef-DOCUMENT",
                "relationshipType": "DESCRIBES",
                "relatedSpdxElement": root_package["SPDXID"],
            },
            *[
                {
                    "spdxElementId": root_package["SPDXID"],
                    "relationshipType": "DEPENDS_ON",
                    "relatedSpdxElement": dependency["SPDXID"],
                }
                for dependency in dependencies
            ],
        ],
    }


def serialized_sbom() -> str:
    return json.dumps(generate(), indent=2, ensure_ascii=False) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate the checked-in SPDX SBOM.")
    parser.add_argument(
        "--check",
        action="store_true",
        help="fail if the checked-in SBOM differs from generated content",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help="output path (default: SBOM.spdx.json)",
    )
    args = parser.parse_args()
    content = serialized_sbom()

    if args.check:
        if not args.output.exists() or args.output.read_text(encoding="utf-8") != content:
            raise SystemExit(
                f"{args.output.relative_to(ROOT)} is stale; run "
                "python tools/generate_sbom.py"
            )
        print(f"{args.output.relative_to(ROOT)} is current.")
        return

    args.output.write_text(content, encoding="utf-8", newline="\n")
    print(f"Wrote {args.output.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
