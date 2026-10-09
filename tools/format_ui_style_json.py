#!/usr/bin/env python3
"""Rewrite UI style JSON with compact scalar arrays.

Do not use json.dumps(..., indent=2) on these files — it expands every
array element onto its own line. Prefer this script (or the same layout
rules) when rewriting style.json.

  python tools/format_ui_style_json.py

Formats:
  config/ui/style.json
  tests/fixtures/ui/style.json

Scalar arrays (layouts, colours, hotspots, …) keep values on one line:

  "fullscreen": [
    0.0, 0.0, 1.0, 1.0
  ],
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STYLE_FILES = (
    ROOT / "config" / "ui" / "style.json",
    ROOT / "tests" / "fixtures" / "ui" / "style.json",
)


def is_scalar_list(value: object) -> bool:
    return isinstance(value, list) and all(
        isinstance(item, (int, float, str, bool)) or item is None for item in value
    )


def dump(value: object, indent: int = 0) -> str:
    pad = "  " * indent
    inner = "  " * (indent + 1)
    if isinstance(value, dict):
        if not value:
            return "{}"
        lines = ["{"]
        items = list(value.items())
        for index, (key, child) in enumerate(items):
            comma = "," if index + 1 < len(items) else ""
            key_text = json.dumps(key)
            if is_scalar_list(child):
                elements = ", ".join(json.dumps(item) for item in child)
                lines.append(f"{inner}{key_text}: [")
                lines.append(f"{inner}  {elements}")
                lines.append(f"{inner}]{comma}")
            else:
                rendered = dump(child, indent + 1)
                lines.append(f"{inner}{key_text}: {rendered}{comma}")
        lines.append(f"{pad}}}")
        return "\n".join(lines)
    if isinstance(value, list):
        if is_scalar_list(value):
            elements = ", ".join(json.dumps(item) for item in value)
            return f"[\n{inner}{elements}\n{pad}]"
        if not value:
            return "[]"
        lines = ["["]
        for index, child in enumerate(value):
            comma = "," if index + 1 < len(value) else ""
            lines.append(f"{inner}{dump(child, indent + 1)}{comma}")
        lines.append(f"{pad}]")
        return "\n".join(lines)
    return json.dumps(value)


def format_file(path: Path) -> None:
    data = json.loads(path.read_text(encoding="utf-8"))
    path.write_text(dump(data) + "\n", encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    del argv  # unused; keep CLI shape consistent with other tools
    for path in STYLE_FILES:
        if not path.is_file():
            raise FileNotFoundError(path)
        format_file(path)
        print(f"formatted {path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, json.JSONDecodeError) as error:
        sys.exit(str(error))
