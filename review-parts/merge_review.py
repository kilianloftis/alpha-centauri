#!/usr/bin/env python3
"""Merge review-parts/*.md into docs/full-code-review.md"""
from __future__ import annotations

import datetime
import re
from pathlib import Path

repo = Path(__file__).resolve().parents[1]
parts_dir = repo / "review-parts"
out = repo / "docs" / "full-code-review.md"

ids = [
    "00-architecture",
    "01-game-core-state",
    "02-game-core-turn",
    "03-buildings",
    "04-council-runtime",
    "05-council-config",
    "06-effects-core",
    "07-effects-rules",
    "08-faction-economy",
    "09-faction-military",
    "10-base-manager",
    "11-base-population-production",
    "12-base-resources",
    "13-map-runtime",
    "14-map-generation",
    "15-orbital",
    "16-population-calculators",
    "17-population-types",
    "18-research",
    "19-social-engineering",
    "20-stages-early",
    "21-stages-late",
    "22-units-core-movement",
    "23-units-combat-probes",
    "24-graphics",
    "25-input",
    "26-lib",
    "27-ui-core",
    "28-ui-base-displays",
    "29-ui-base-selectors",
    "30-ui-commlinks",
    "31-ui-council",
    "32-ui-research",
    "33-ui-satellite",
    "34-ui-settings",
    "35-ui-social-engineering",
    "36-ui-style",
    "37-ui-unit-designer",
    "38-ui-world-map",
    "39-ui-world-panels",
]


def counts(text: str) -> tuple[int, int, int]:
    h = len(re.findall(r"^### \[H\]", text, re.M))
    m = len(re.findall(r"^### \[M\]", text, re.M))
    low = len(re.findall(r"^### \[L\]", text, re.M))
    return h, m, low


def main() -> None:
    missing: list[str] = []
    parts: list[Path] = []
    for i in ids:
        path = parts_dir / f"{i}.md"
        if not path.exists():
            missing.append(i)
            continue
        parts.append(path)

    if missing:
        raise SystemExit("Missing part files:\n" + "\n".join(missing))

    total = [0, 0, 0]
    toc: list[str] = []
    bodies: list[str] = []
    for p in parts:
        text = p.read_text().strip() + "\n"
        h, m, low = counts(text)
        total[0] += h
        total[1] += m
        total[2] += low
        mheading = re.search(r"^## (.+)$", text, re.M)
        title = mheading.group(1) if mheading else p.stem
        anchor = re.sub(r"[^a-z0-9]+", "-", title.lower()).strip("-")
        toc.append(f"- [{title}](#{anchor}) — H={h} M={m} L={low}")
        bodies.append(text)

    today = datetime.date.today().isoformat()
    header = f"""# Full project code review

**Date:** {today}
**Scope:** All of `src/` except `Engine` (ad-hoc testing catchall). Headers under `include/` reviewed with their implementations.
**Lens:** clean code, simplicity, SOLID, clarity, robustness, maintainability.
**Note:** Many systems are intentionally unimplemented; missing features are not treated as defects.

## Summary

| Severity | Count |
|----------|------:|
| High `[H]` | {total[0]} |
| Medium `[M]` | {total[1]} |
| Low `[L]` | {total[2]} |
| **Total** | **{sum(total)}** |

Parts were produced by per-directory review agents (directories with >8 implementation files split across two agents), plus one architecture-only pass.

## Contents

{chr(10).join(toc)}

---

"""

    out.parent.mkdir(exist_ok=True)
    out.write_text(header + "\n---\n\n".join(bodies))
    print(f"Wrote {out} ({out.stat().st_size} bytes)")
    print(f"Totals: H={total[0]} M={total[1]} L={total[2]}")


if __name__ == "__main__":
    main()
