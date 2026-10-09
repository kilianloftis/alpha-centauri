#!/usr/bin/env python3
"""
Run every in-repo SMAC asset extractor into the expected assets/ tree.

Requires a local Sid Meier's Alpha Centauri install. Extracted binaries are
gitignored; see the graphics assets roadmap.

Usage:
    python extract_all.py
    python extract_all.py --game-dir "/path/to/Sid Meier's Alpha Centauri"
    python extract_all.py --skip terrain,fonts
"""

from __future__ import annotations

import argparse
import importlib
import sys
from pathlib import Path

_LOCAL_GAME_DIR = Path(
    "/home/martok/.PlayOnLinux/wineprefix/AlphaCentauri_gog/"
    "drive_c/GOG Games/Sid Meier's Alpha Centauri"
)
_WINDOWS_GAME_DIR = Path(r"C:\Program Files (x86)\Alpha Centauri")
DEFAULT_GAME_DIR = _LOCAL_GAME_DIR if _LOCAL_GAME_DIR.is_dir() else _WINDOWS_GAME_DIR

# (cli name, module, argv builder). Order matches the graphics assets roadmap.
EXTRACTORS = (
    ("terrain", "extract_terrain", lambda game, out: ["--game-dir", str(game), "--out", str(out)]),
    (
        "faction",
        "extract_faction",
        lambda game, out: ["--all", "--game-dir", str(game), "--out", str(out / "factions")],
    ),
    (
        "fonts",
        "extract_fonts",
        lambda game, out: ["--game-dir", str(game), "--out", str(out / "ui" / "fonts")],
    ),
    ("icons", "extract_icons", lambda game, out: ["--game-dir", str(game), "--out", str(out)]),
    ("ui", "extract_ui", lambda game, out: ["--game-dir", str(game), "--out", str(out)]),
)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "--game-dir",
        type=Path,
        default=DEFAULT_GAME_DIR,
        help=f"SMAC install (default: {DEFAULT_GAME_DIR})",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=Path("assets"),
        help="asset root (default: assets)",
    )
    parser.add_argument(
        "--skip",
        default="",
        help="comma-separated extractor names to skip (terrain,faction,fonts,icons,ui)",
    )
    args = parser.parse_args(argv)

    if not args.game_dir.is_dir():
        raise FileNotFoundError(f"Game directory not found: {args.game_dir}")

    skip = {name.strip() for name in args.skip.split(",") if name.strip()}
    unknown = skip - {name for name, _, _ in EXTRACTORS}
    if unknown:
        raise ValueError(f"Unknown --skip names: {', '.join(sorted(unknown))}")

    for name, module_name, build_argv in EXTRACTORS:
        if name in skip:
            print(f"skip {name}")
            continue
        print(f"=== {name} ===")
        module = importlib.import_module(module_name)
        code = module.main(build_argv(args.game_dir, args.out))
        if code:
            return code
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, ValueError, OSError) as error:
        sys.exit(str(error))
