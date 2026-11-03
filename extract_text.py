#!/usr/bin/env python3
"""
Extract SMAC's talk text from a local install into a text pack under assets/text/<locale>/.

The original game text is not redistributable, so this script reads it from a local SMAC
installation at setup time. Also invoked by extract_all.py.

Usage:
    python extract_text.py
    python extract_text.py --game-dir "/path/to/Sid Meier's Alpha Centauri" --locale en

Output contract (gitignored; format in docs/architecture/ui-system.md, "Text packs"):
    script.json         every xscript.txt entry: {"body": str, "items": [str]}
    labels.json         labels.txt #LABELS by number
    factions/<id>.json  each faction file's header fields and named phrase lines

The text keeps SMAC's markup ($TITLE0, $<1:his:her>, {highlight}); the game parses it the way
terranx.exe does (docs/thinker/terranx-map.md, "Talk").
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from extract_faction_ids import faction_id_for_stem

_LOCAL_GAME_DIR = Path(
    "/home/martok/.PlayOnLinux/wineprefix/AlphaCentauri_gog/"
    "drive_c/GOG Games/Sid Meier's Alpha Centauri"
)
_WINDOWS_GAME_DIR = Path(r"C:\Program Files (x86)\Alpha Centauri")
DEFAULT_GAME_DIR = _LOCAL_GAME_DIR if _LOCAL_GAME_DIR.is_dir() else _WINDOWS_GAME_DIR
DEFAULT_OUT = Path("assets/text")
DEFAULT_LOCALE = "en"

# Alien Crossfire's script; its labels are a superset of script.txt's.
SCRIPT_FILE = "xscript.txt"
LABELS_FILE = "labels.txt"
RULES_FILE = "alphax.txt"
FACTION_SECTIONS = ("FACTIONS", "NEWFACTIONS")

# read_faction: header line → MFaction fields (Thinker names), 1-based after #KEY.
LEADER_LINE_FIELDS = ("title_leader", "adj_leader", "adj_insult_leader", "adj_faction",
                      "adj_insult_faction")
FIRST_PHRASE_LINE = 9
# The phrase lines in order, named for the sentences facedit.txt's #SENTENCES says they complete
# (docs/thinker/terranx-map.md, "Talk").
PHRASE_KEYS = ("goal", "intent", "agenda", "your_agenda", "misdeed", "pastime", "your_pastime",
               "accusation", "methods", "projects", "pursuits", "tribute", "service",
               "enforcers", "code")


def find_file(game_dir: Path, name: str) -> Path:
    """Install file names mix cases (GAIANS.TXT, angels.txt)."""
    for candidate in game_dir.iterdir():
        if candidate.name.lower() == name.lower():
            return candidate
    raise FileNotFoundError(f"Missing {name} in install: {game_dir}")


def read_lines(path: Path) -> list[str]:
    # SMAC's text is Windows-1252 (0x92 apostrophes).
    return path.read_bytes().decode("cp1252").replace("\r", "").split("\n")


def is_label(line: str) -> bool:
    return line.startswith("#") and len(line) > 1 and (line[1].isupper() or line[1].isdigit())


def is_directive(line: str) -> bool:
    return line.startswith("#") and len(line) > 1 and line[1].islower()


def sections(lines: list[str]) -> dict[str, list[str]]:
    """Each #LABEL's lines up to the next label, without ; and ## comments."""
    found: dict[str, list[str]] = {}
    current: list[str] | None = None
    for line in lines:
        if line.startswith(";") or line.startswith("##"):
            continue
        if is_label(line):
            current = found.setdefault(line[1:].strip(), [])
            continue
        if current is not None:
            current.append(line)
    return found


def script_entry(lines: list[str]) -> dict:
    """A popup (it has #directives) is a body up to the first blank line, then items; an item
    list like DIPLOMENU is all items, read line by line."""
    content = [line.rstrip() for line in lines if not is_directive(line)]
    if not any(is_directive(line) for line in lines):
        return {"body": "", "items": [line for line in content if line]}
    body: list[str] = []
    items: list[str] = []
    seen_blank = False
    for line in content:
        if not line:
            seen_blank = seen_blank or bool(body)
            continue
        (items if seen_blank else body).append(line)
    return {"body": "\n".join(body), "items": items}


def extract_script(game_dir: Path) -> dict:
    found = sections(read_lines(find_file(game_dir, SCRIPT_FILE)))
    return {label: script_entry(lines) for label, lines in found.items()}


def extract_labels(game_dir: Path) -> dict:
    lines = sections(read_lines(find_file(game_dir, LABELS_FILE)))["LABELS"]
    count = int(lines[0].strip())
    return {str(index): text for index, text in enumerate(lines[1:count + 1])}


def items_of(line: str) -> list[str]:
    return [item.strip() for item in line.split(",")]


def faction_vocabulary(lines: list[str], context: str) -> dict:
    header = [line.strip() for line in lines]
    names = items_of(header[0])
    leader = items_of(header[6])
    noun_number = int(names[4]) if names[4].lstrip("-").isdigit() else 1
    phrases: list[str] = []
    for line in header[FIRST_PHRASE_LINE - 1:]:
        if not line:
            break
        phrases.append(line)
    if len(phrases) != len(PHRASE_KEYS):
        raise ValueError(
            f"{context}: {len(phrases)} phrase lines, expected {len(PHRASE_KEYS)}")
    fields = {
        "formal_name_faction": names[0],
        "desc_name_faction": names[1],
        "noun_faction": names[2],
        "name_leader": names[5],
        "adj_name_faction": items_of(header[4])[0],
        "insult_leader": items_of(header[7])[0],
    }
    fields.update(zip(LEADER_LINE_FIELDS, leader))
    return {
        "leader_female": names[6][:1].upper() == "F",
        "noun_gender": names[3][:1].upper() if names[3][:1].upper() in ("F", "N") else "M",
        "noun_plural": noun_number >= 2,
        "fields": fields,
        "phrases": dict(zip(PHRASE_KEYS, phrases)),
    }


def extract_factions(game_dir: Path) -> dict[str, dict]:
    rules = sections(read_lines(find_file(game_dir, RULES_FILE)))
    vocabularies: dict[str, dict] = {}
    for section in FACTION_SECTIONS:
        for line in rules[section]:
            if not line.strip():
                break
            file_stem, key = items_of(line)[:2]
            faction_file = find_file(game_dir, f"{file_stem}.txt")
            vocabularies[faction_id_for_stem(file_stem)] = faction_vocabulary(
                sections(read_lines(faction_file))[key], f"{faction_file.name} #{key}")
    return vocabularies


def write_json(path: Path, value) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def extract_text(game_dir: Path, out_dir: Path, locale: str) -> list[Path]:
    if not game_dir.is_dir():
        raise FileNotFoundError(f"Game directory not found: {game_dir}")
    locale_dir = out_dir / locale
    written = [locale_dir / "script.json", locale_dir / "labels.json"]
    write_json(written[0], extract_script(game_dir))
    write_json(written[1], extract_labels(game_dir))
    for faction_id, vocabulary in extract_factions(game_dir).items():
        path = locale_dir / "factions" / f"{faction_id}.json"
        write_json(path, vocabulary)
        written.append(path)
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
        help=f"text pack root (default: {DEFAULT_OUT})",
    )
    parser.add_argument(
        "--locale",
        default=DEFAULT_LOCALE,
        help=f"language of the install's text (default: {DEFAULT_LOCALE})",
    )
    args = parser.parse_args(argv)

    written = extract_text(args.game_dir, args.out, args.locale)
    print(f"text: {len(written)} files -> {args.out / args.locale}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, KeyError, ValueError, OSError) as error:
        sys.exit(str(error))
