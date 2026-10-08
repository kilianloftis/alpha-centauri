#!/usr/bin/env python3
"""
Copy SMAC TrueType faces from a local install into assets/ui/fonts/.

The original game assets are not redistributable, so this script copies them from a
local SMAC installation at setup time. Wire into extract_all.py when that
orchestrator lands (see graphics assets roadmap).

Usage:
    python extract_fonts.py
    python extract_fonts.py --game-dir "/path/to/Sid Meier's Alpha Centauri"

Install faces (terranx.exe loads Arial Narrow; ALPHC is bundled but unused by the exe):
  arialn.ttf / arialnb.ttf / arialni.ttf / arialnbi.ttf
  ALPHC___.TTF → alphc.ttf

After extract, graphics.font_paths should prefer assets/ui/fonts/arialn.ttf, then
system fallbacks (DejaVu/Liberation). Bold/italic copies are available for later
wiring; runtime still loads one face via font_paths.
"""

from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

# Prefer this machine's GOG/PlayOnLinux install; fall back to the stock Windows path.
_LOCAL_GAME_DIR = Path(
    "/home/martok/.PlayOnLinux/wineprefix/AlphaCentauri_gog/"
    "drive_c/GOG Games/Sid Meier's Alpha Centauri"
)
_WINDOWS_GAME_DIR = Path(r"C:\Program Files (x86)\Alpha Centauri")
DEFAULT_GAME_DIR = _LOCAL_GAME_DIR if _LOCAL_GAME_DIR.is_dir() else _WINDOWS_GAME_DIR
DEFAULT_OUT = Path("assets/ui/fonts")

# (install relative name, output basename). All required.
FONT_COPIES = (
    ("arialn.ttf", "arialn.ttf"),
    ("arialnb.ttf", "arialnb.ttf"),
    ("arialni.ttf", "arialni.ttf"),
    ("arialnbi.ttf", "arialnbi.ttf"),
    ("ALPHC___.TTF", "alphc.ttf"),
)


def extract_fonts(game_dir: Path, out_dir: Path) -> list[Path]:
    if not game_dir.is_dir():
        raise FileNotFoundError(f"Game directory not found: {game_dir}")

    out_dir.mkdir(parents=True, exist_ok=True)
    written: list[Path] = []
    for source_name, dest_name in FONT_COPIES:
        source = game_dir / source_name
        if not source.is_file():
            raise FileNotFoundError(f"Missing font in install: {source}")
        destination = out_dir / dest_name
        shutil.copy2(source, destination)
        written.append(destination)
    return written


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
        default=DEFAULT_OUT,
        help=f"output directory (default: {DEFAULT_OUT})",
    )
    args = parser.parse_args(argv)

    written = extract_fonts(args.game_dir, args.out)
    print(f"fonts: {len(written)} files -> {args.out}")
    for path in written:
        print(f"  {path}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, OSError) as error:
        sys.exit(str(error))
