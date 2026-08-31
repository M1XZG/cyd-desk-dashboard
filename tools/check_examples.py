#!/usr/bin/env python3

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "firmware/src/main.cpp"

EXAMPLES = {
    "kConfigExample": ROOT / "sd-card/dashboard/config.example.json",
    "kConnectionsExample": ROOT / "sd-card/dashboard/connections.example.json",
}


def embedded_json(source: str, name: str) -> object:
    pattern = rf'constexpr const char {name}\[\] = R"json\((.*?)\)json";'
    match = re.search(pattern, source, re.DOTALL)
    if match is None:
        raise RuntimeError(f"Could not find {name} in {SOURCE}")
    return json.loads(match.group(1))


def main() -> None:
    source = SOURCE.read_text(encoding="utf-8")
    failed = False
    for name, path in EXAMPLES.items():
        embedded = embedded_json(source, name)
        repository_copy = json.loads(path.read_text(encoding="utf-8"))
        if embedded != repository_copy:
            print(f"{path.relative_to(ROOT)} differs from {name}")
            failed = True
    if failed:
        raise SystemExit(1)
    print("Firmware and SD-card examples match.")


if __name__ == "__main__":
    main()
