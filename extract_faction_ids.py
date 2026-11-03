"""
SMAC file stems ↔ this game's faction ids, shared by the faction extractors.

Faction art (gaians.pcx) and faction text (GAIANS.TXT) share a stem. Runtime uses the
config/factions/<id> name (gaian), so assets land under that id with no table in C++.
"""

from __future__ import annotations

# SMAC stem → config/factions/<id>. Unlisted stems keep their (lowercased) name.
STEM_TO_FACTION_ID = {
    "gaians": "gaian",
    "univ": "university",
}
FACTION_ID_TO_STEM = {faction_id: stem for stem, faction_id in STEM_TO_FACTION_ID.items()}


def faction_id_for_stem(stem: str) -> str:
    return STEM_TO_FACTION_ID.get(stem.lower(), stem.lower())
