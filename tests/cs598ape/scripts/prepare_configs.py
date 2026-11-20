#!/usr/bin/env python3

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
CONFIG_DIR = ROOT / "configs"
TRACE_DIR = ROOT / "traces"
OUTPUT_DIR = ROOT / "generated"

OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

configs = sorted(CONFIG_DIR.glob("*.yaml"))

if len(configs) != 9:
    raise RuntimeError(
        f"Expected 9 YAML configurations, found {len(configs)}"
    )

pattern = re.compile(
    r"(?m)^([ \t]*-[ \t]+)([^\r\n]+\.trace)([ \t]*)$"
)

for config in configs:
    content = config.read_text(encoding="utf-8")
    count = [0]

    def replace_trace(match):
        filename = Path(match.group(2).strip()).name
        target = TRACE_DIR / filename

        if not target.is_file():
            raise FileNotFoundError(target)

        count[0] += 1
        return f"{match.group(1)}{target}{match.group(3)}"

    updated = pattern.sub(replace_trace, content)

    if count[0] == 0:
        raise RuntimeError(
            f"No trace references found in {config.name}"
        )

    output = OUTPUT_DIR / config.name
    output.write_text(updated, encoding="utf-8")

    print(
        f"PASS: {config.name} "
        f"({count[0]} trace reference(s))"
    )

print(f"\nSUCCESS: {len(configs)} configurations prepared.")
