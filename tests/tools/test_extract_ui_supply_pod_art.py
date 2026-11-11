#!/usr/bin/env python3
"""Supply-pod goody thumb mapping stays aligned with shipping notices."""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

import extract_ui  # noqa: E402


def _event_groups(cfg: dict) -> list[dict]:
    return cfg["event_groups"]


def _domain(event_group: dict) -> str:
    return event_group["placement"]["domain"]


class SupplyPodNoticeArtTests(unittest.TestCase):
    def test_every_shipping_event_is_in_the_art_table(self) -> None:
        cfg = json.loads((ROOT / "config/supply_pods.json").read_text())
        for event_group in _event_groups(cfg):
            for event in event_group["events"]:
                self.assertIn(event["id"], extract_ui.SUPPLY_POD_NOTICE_ART)

    def test_shipping_images_match_the_art_table(self) -> None:
        cfg = json.loads((ROOT / "config/supply_pods.json").read_text())
        for event_group in _event_groups(cfg):
            surface = "land" if _domain(event_group) == "Land" else "sea"
            for event in event_group["events"]:
                art = extract_ui.SUPPLY_POD_NOTICE_ART[event["id"]]
                image = event["notice"].get("image")
                stem = art[surface]
                expected = extract_ui.thumb_asset_path(stem) if stem else None
                self.assertEqual(image, expected, f"{event['id']} {surface}")

    def test_required_thumbs_include_spore_and_ogre_variants(self) -> None:
        stems = extract_ui.supply_pod_thumb_stems()
        self.assertIn("sporlnch_sm", stems)
        self.assertIn("ogre_sm", stems)
        self.assertIn("mindworm_sm", stems)


if __name__ == "__main__":
    unittest.main()
