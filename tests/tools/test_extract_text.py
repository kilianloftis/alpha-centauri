"""Tests for extract_text.py over the synthetic install in tests/fixtures/smac_text/."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT))

import extract_text  # noqa: E402

FIXTURE_INSTALL = REPO_ROOT / "tests" / "fixtures" / "smac_text"


class ExtractTextTests(unittest.TestCase):
    def setUp(self) -> None:
        self._out = tempfile.TemporaryDirectory()
        extract_text.extract_text(FIXTURE_INSTALL, Path(self._out.name), "en")
        self.locale = Path(self._out.name) / "en"

    def tearDown(self) -> None:
        self._out.cleanup()

    def load(self, relative: str):
        return json.loads((self.locale / relative).read_text(encoding="utf-8"))

    def test_a_popup_splits_body_from_items_and_drops_directives(self) -> None:
        script = self.load("script.json")
        self.assertEqual(
            script["GREETING"],
            {"body": '"Hello, $TITLE0 $NAME1.\nIt’s a fine day."',
             "items": ["Answer one.", "Answer two."]},
        )
        self.assertEqual(script["PLAIN"], {"body": "Just a body.", "items": []})

    def test_an_entry_without_directives_is_all_items(self) -> None:
        self.assertEqual(
            self.load("script.json")["CHOICES"],
            {"body": "", "items": ['"First line."', '"Second line."']},
        )

    def test_labels_are_numbered_from_zero_after_the_count(self) -> None:
        self.assertEqual(self.load("labels.json"), {"0": "Zero", "1": "One", "2": "Two"})

    def test_faction_header_fields_and_phrases(self) -> None:
        gaian = self.load("factions/gaian.json")
        self.assertTrue(gaian["leader_female"])
        self.assertEqual(gaian["noun_gender"], "F")
        self.assertTrue(gaian["noun_plural"])
        self.assertEqual(gaian["fields"], {
            "formal_name_faction": "Formal Name",
            "desc_name_faction": "The Short",
            "noun_faction": "Nouns",
            "name_leader": "Leaderina",
            "adj_name_faction": "Adjective",
            "insult_leader": "Insult",
            "title_leader": "Title",
            "adj_leader": "kind",
            "adj_insult_leader": "mean",
            "adj_faction": "good",
            "adj_insult_faction": "bad",
        })
        self.assertEqual(list(gaian["phrases"]), list(extract_text.PHRASE_KEYS))
        self.assertEqual(gaian["phrases"]["goal"], "first phrase")
        self.assertEqual(gaian["phrases"]["intent"], "second, with a comma")
        self.assertEqual(gaian["phrases"]["methods"], "tagged noun, F2")

    def test_a_faction_without_fifteen_phrase_lines_is_rejected(self) -> None:
        header = ["Name, Short, Noun, M, 1, Leader, M", "", "", "", "Adj,", "",
                  "Title, a, b, c, d,", "Insult", "lonely phrase"]
        with self.assertRaisesRegex(ValueError, "LONELY.TXT #LONELY: 1 phrase lines"):
            extract_text.faction_vocabulary(header, "LONELY.TXT #LONELY")

    def test_file_names_match_in_any_case_and_stems_map_to_faction_ids(self) -> None:
        testers = self.load("factions/testers.json")
        self.assertFalse(testers["leader_female"])
        self.assertEqual(testers["noun_gender"], "M")
        self.assertFalse(testers["noun_plural"])
        self.assertEqual(testers["phrases"]["code"], "testers code")
        self.assertFalse((self.locale / "factions" / "gaians.json").exists())


if __name__ == "__main__":
    unittest.main()
