---
name: Commlinks talk
overview: Clicking a met faction on the Commlinks panel opens a talk popup driven by data. The flow — scenes, choices, the action each choice runs, and which text answers each result — lives in config/ui/talk.json and names text by id. The text lives apart, in per-locale text packs that use SMAC's own script markup. extract_text.py extracts SMAC's script, labels and faction vocabulary from the user's install into a gitignored pack. An in-repo English pack covers every id, so the game runs without extraction. Actions, conditions, generators and value sources are closed C++ vocabularies over the rules. GetTalkActions decides what talk offers, proposals go through DiplomaticActionExecutor::Propose, and renouncing a Pact goes through CancelTreaty, whose blemish matches SMAC's inline +1.
todos:
  - id: re-record
    content: "terranx-map.md Talk: confirm and record parse_string markup, slot gender/plural, faction-file header layout, get_phrase / get_noun_phrase / get_pact_hood, each talk scene's slot bindings, the human rival's script file, make_a_proposal item 3 at Vendetta, the remaining proposal_menu gates"
    status: completed
  - id: game-rules
    content: GetTalkActions (DiplomacyActions), public CanDeliver (DiplomaticProposalRules)
    status: completed
  - id: extract-text
    content: extract_text.py (script.json, labels.json, factions/<id>.json under assets/text/<locale>/); register in extract_all.py; gitignore assets/text/
    status: completed
  - id: text-packs
    content: TextPack (ordered roots, per-entry overlay, locale), ScriptMarkup (SMAC markup → runs, slot scan), in-repo config/text/en pack; delete phrases.json, ParsePhrases, PickPhrase, Format
    status: completed
  - id: talk-script
    content: config/ui/talk.json + TalkScript (parse, validate against TextPack, faction registry and the C++ vocabularies)
    status: completed
  - id: captions-floor
    content: standing_captions.json vendetta_mood_floor; StandingCaptions::CaptionMood(friction, status)
    status: completed
  - id: talk-ui
    content: TalkBindings, TalkConversation (interpreter), WrapText over runs, TalkPopup, TalkPopupStyle_t; CommlinksPanel row clicks; Engine / ViewFactory / CommlinksView wiring
    status: completed
  - id: deviations-docs
    content: Deviations entries; ui-system.md text packs and talk; diplomacy-system.md; graphics assets roadmap extractor table; refactor-flags.md
    status: completed
  - id: tests-verify
    content: Markup, TextPack, TalkScript validation, extractor, rules, panel click, TalkPopup tests; ./bd test; run extract_text.py and boot against both packs; screenshots of the talk popup
    status: in_progress
isProject: false
---

# Commlinks talk

## Context

Slice 5 of [finish_diplomacy_roadmap_6ecba412.plan.md](.claude/plans/finish_diplomacy_roadmap_6ecba412.plan.md).
The Commlinks panel lists met factions and nothing on it can be clicked. `DiplomaticActionExecutor`,
`GetAvailableActions`, `TradeKinds` and `CancelTreaty` have no production caller
(`docs/plans/refactor-flags.md`). This slice adds the face-to-face channel that calls them.

SMAC keeps talk text in markup (`xscript.txt`) and its flow in code (`diplomacy_menu`,
`proposal_menu`, `communicate`). Here the flow is data too. The split is three layers:

- **Flow:** `config/ui/talk.json`. Scenes, choices, the action a choice runs, the text that
  answers each result, and which value fills each text slot. It holds text ids, never copy.
- **Text:** per-locale packs in SMAC's own markup, so SMAC's script and a translated script load
  the same way.
- **Vocabulary:** C++. A closed set of actions, conditions, generators and value sources, each
  over the existing rules. The flow can arrange them but cannot make anything legal that a rule
  forbids.

Every AI still accepts (`EvaluateResponse_`).

## Confirmed in the binary

`terranx.exe` `sha1` `4b19c1fe3266b5ebc4305cd182ed6e864e3a1c4a`. Add to
[terranx-map.md](docs/thinker/terranx-map.md) "Talk":

- **Greeting** (`introduce` `0x53D360`, called by `wants_to_speak` at `0x53E240` as
  `introduce(player, rival)`).
  - Slots: 0 player `title_leader`, 1 player `name_leader`, 2 rival `title_leader`, 3 rival
    `name_leader`, 4 rival `noun_faction`, 8 rival `adj_name_faction`, 9 player `adj_leader`.
    Each `parse_says` first sets the slot's gender (`0x9BBFEC`) and plural (`0x9BBFF0`).
  - The label is `INTRO` + suffix + a random index, chosen from `diplo_status[player][rival]`
    (`0x53D4F1`):
    - `0x8000000` unset, or a multiplayer game with a human rival: `INTRONEW`, `random(6)`
      (`0x53D6D6`). Slot 6 is `get_phrase(rival, 1)`.
    - else `DIPLO_WANT_REVENGE` (`0x53D525`): `INTROMAD`, `random(5)`.
    - else `INTRO`, `random(4)` (`0x53D60B`).
  - `INTRONEW2` sets slot 5 to the player's `noun_faction` (`0x53D83E`). `INTRONEW3` sets it to
    "a" or "an" before a rank word from `7 - ranking` (`0x53D725`–`0x53D834`).
  - `0x8000000` is set only by `communicate`'s AI-initiated introduction and betrayal branches
    (`INTRODUCEENEMY*`, `BETRAYFRIEND`): `treaty_on(…, COMMLINK|0x8000000)` at `0x5522CC`,
    `0x5524D1`, `0x552745` and `0x5529C6`. No other immediate in the binary sets it. Thinker's
    `whose_territory` requires it beside the commlink. A faction met by contact always greets
    from `INTRONEW`.
- **Diplomacy menu** (`diplomacy_menu` `0x54C560`, `(player, rival)`). It shows `DIPLOMENU`'s
  lines as item ids:
  - 0 finished: always.
  - 1 trade again on the same terms: after a trade this session (`0x93FAD0`).
  - 2 make a proposal: always. Line 3, "another proposal", replaces line 2 once one was made
    (`0x93FAD4`).
  - 3 renounce the Pact: `diplo_status[rival][player] & DIPLO_PACT` (`0x54C8A7`).
  - 4 demand withdrawal from territory: `diplo_status[player][rival] & 7` (`0x54C8F4`).
  - 5 battle plans: single player, `0x93FA94 > 0`.
  - 6 call off a Vendetta against a friend: single player, `0x93F7DC > 0`.
  - 7 council vote: `can_call_council`, and neither faction has rule flag `0x80` (`0x946F58`).

  Item ids are not line positions: lines 4–8 carry ids 5, 6, 7, 3 and 4. `talk.json`'s `item`
  is the line position. No item declares Vendetta or ends a Truce or Treaty. Those happen at an
  attack (`break_treaty`).
- **Renounce** (`communicate` `0x54FFD0`, item 3):
  - `REALLYBROKE` (`0x5585C4`) asks the player to confirm. Its item 0 backs down and plays
    `DROIDS`.
  - Item 1 confirms: the player's blemishes `+ 1` inline (`0x55861B`–`0x55862B`),
    `net_treaty_off` (`0x558631`), a caption refresh, `BROKEPACT` (`0x5586A0`) and
    `net_pact_ends` (`0x5586AE`).
- **Proposal menu** (`proposal_menu` `0x54DCF0`):
  - 0 never mind and 1 gift: always.
  - 2 Pact: the pair holds a Treaty and no Pact (`0x54E012`).
  - 3 Treaty: the pair holds no Treaty (`0x54E04E`), Vendetta included.
  - 4 research data: unless game rule `0x2000000` (`0x9A649C`).
- **Caption** (`diplomacy_caption` `0x5399A0`):
  - At Vendetta with cached friction below 15, it sets the cached `DiploFriction` (`0x93FA74`)
    to 15 at `0x5399DE`. The stored friction does not change.
  - The caption is the rival's `noun_faction`, `" - "` (`0x539A90`) and the mood.
- **Replies**: `MAKETRUCE`, `MAKETREATY0` / `1` and `MAKEPACT` accept. `REJPACT*` and
  `REJTREATY*` refuse.
- **Faction text**:
  - Each faction file's header is positional. Its lines are names, genders and plural flags,
    title and adjectives (Thinker's `MFaction` names them), then phrase lines, some tagged
    `M1` / `M2`.
  - `get_phrase(faction, n)` and `get_noun_phrase(faction, n)` index those phrase lines.
  - `#FACTIONTRUCE` / `#FACTIONTREATY` are that AI's own offers, which arrive with AI
    initiation.
- **Labels**: `labels.txt` `#LABELS` is a count, then one label per line. Talk's portrait uses
  932 `REPUTATION` and 933 `MOOD`, and the InfoWin uses 222 `Integrity`.

### Also recorded

`parse_string`'s grammar, the slot tables (`parse_says` / `parse_num`), the faction-file
header and `get_phrase` / `get_noun_phrase` / `get_pact` / `get_pact_hood`, the slot bindings
at every call site `talk.json` uses, and the default script file are in
[terranx-map.md](docs/thinker/terranx-map.md), "Talk". Two findings shape the design:

- The renounce blemish matches `CancelTreaty`, which already writes `break_agreement_blemishes`
  (1), so it is unchanged.
- The caption names the faction, not the leader.

## Game layer

[DiplomacyActions.h](include/game/diplomacy/DiplomacyActions.h):

```cpp
// What SMAC's diplomacy menu offers face to face, out of GetAvailableActions: proposals,
// trade, and cancelling only an agreement that steps down to another (renouncing a Pact).
std::vector<DiplomaticActionKind_t> GetTalkActions(const DiplomacyLedger& rLedger,
                                                   FactionId_t a,
                                                   FactionId_t b);
```

It returns `GetAvailableActions` without `DeclareVendetta`. It keeps `CancelTreaty` only when
`StepDown` of the pair's status is itself an agreement (`IsAgreement`).

[DiplomaticProposalRules.h](include/game/diplomacy/DiplomaticProposalRules.h):

```cpp
// Whether giver can hand rItem to receiver; IsValidProposal adds the whole-side checks.
bool CanDeliver(FactionId_t giver, FactionId_t receiver, const TradeItem_t& rItem);
```

It visits the existing `CanDeliver_` on `CurrentGameState()`.

## Text packs

### Layout

A pack root holds one directory per locale:

```text
<root>/<locale>/script.json          # scene entries by id
<root>/<locale>/labels.json          # labels by number
<root>/<locale>/factions/<id>.json   # one faction's talk vocabulary
```

[GameDataPaths.h](include/game/GameDataPaths.h) gains
`textRoots = {"assets/text", "config/text"}` and `textLocale = "en"`.

`TextPack` (`include/ui/text/TextPack.h`) loads every root's locale directory that exists. A
missing root is skipped, so the extracted pack is optional. It resolves:

- each script entry and each label from the first root that has it;
- each faction's vocabulary file whole, from the first root that has it, so one file's names
  and genders stay together.

An id that no root has throws.

Mood and repute names (`standing_captions.json`), and faction names elsewhere in the UI, stay
where they are.

### Formats

`script.json`:

```json
{
  "INTRONEW0": {
    "body": "\"Ah, the $CHARACTERADJ9 $TITLE0 $NAME1. I have heard...\"",
    "items": []
  },
  "DIPLOMENU": { "body": "", "items": ["\"I believe we are finished here. $NAME0 out.\"", "..."] }
}
```

- `body` is the entry's lines up to the first blank line, joined with `\n`.
- `items` are the lines after it. An entry with no blank line, like `DIPLOMENU`, is all items,
  which is how `diplomacy_menu` reads it.

`labels.json`: `{ "222": "Integrity", "932": "REPUTATION", "933": "MOOD", … }`.

`factions/<id>.json`:

- `fields`: the header fields under Thinker's `MFaction` names: `name_leader`, `title_leader`,
  `adj_leader`, `adj_insult_leader`, `adj_faction`, `adj_insult_faction`, `noun_faction`,
  `adj_name_faction`, `formal_name_faction`, `desc_name_faction` and `insult_leader`;
- `leader_female`, the grammar of the leader fields;
- `noun_gender` (`M`, `F` or `N`) and `noun_plural`, the grammar of the faction fields;
- `phrases`: the 15 raw lines after `insult_leader`, as `get_phrase` returns them, keyed by the
  sentence each completes in SMAC's faction editor (`facedit.txt` `#SENTENCES`): `goal`,
  `intent`, `agenda`, `your_agenda`, `misdeed`, `pastime`, `your_pastime`, `accusation`,
  `methods`, `projects`, `pursuits`, `tribute`, `service`, `enforcers`, `code`. All 15 are
  required. `get_noun_phrase` reads a line's first comma item and its `M1` / `F2` / `N1` tag at
  use (`NounPhrase`).

Script ids and label numbers stay SMAC's. They are what the game looks up, a translated install
keeps them, and they match `xscript.txt` and `terranx-map.md`; our own entries take `TALK*` ids.
Where a label is used it is named (`talk.json` `portrait` keys, C++ constants).

### extract_text.py

Add `extract_text.py` at the repo root, next to the other extractors and in their style
(docstring, `--game-dir`, `--out`, `main(argv)`):

- `--out` defaults to `assets/text`. `--locale` defaults to `en`, meaning the language of the
  install's text.
- Script: it reads the script file the RE names (`xscript.txt`), skips `;` and `##` lines, and
  splits on `#LABEL` lines. Directives (`#caption`, `#xs`, `#wave`, `#listbox`, `#itemlist`)
  are dropped; the popup draws the caption from the `caption` value source. It writes `script.json` with every entry, not only the
  ones talk uses.
- Labels: it reads `labels.txt` `#LABELS` and writes `labels.json`.
- Factions: it reads each file named in `alphax.txt` `#FACTIONS` and `#NEWFACTIONS`, and writes
  `factions/<id>.json`. The id comes from `extract_faction_ids.faction_id_for_stem`, so
  `GAIANS.TXT` becomes `gaian`, the same id the art uses.
- It reads files as cp1252 (SMAC's apostrophes are `0x92`) with `\r` stripped, matches install
  file names in any case, and writes UTF-8.
- The stem-to-id map moves from `extract_faction.py` into `extract_faction_ids.py`, which both
  extractors import (`extract_faction.py` needs Pillow at import).

Register it in [extract_all.py](extract_all.py)'s `EXTRACTORS` as `text`, and update the
`--skip` help. Add `assets/text/` to [.gitignore](.gitignore) beside the art.

### In-repo English pack

`config/text/en/` is our own wording, in the same markup. It covers every id `talk.json`
references, labels 222, 932 and 933, and vocabulary for `gaian`, `hive` and `university`. The
game therefore runs with no extraction. Where the extracted pack has an id, its SMAC text wins.
Our pack mirrors SMAC's item order for `DIPLOMENU`, `PROPOSALMENU` and `REALLYBROKE`, since the
flow names items by position. It also defines the `TALK*` ids that SMAC lacks:

- trade prompts, kind labels and candidate labels, e.g. `$NUM0 energy credits`;
- the trade accept and refuse lines and the invalid-offer prompt.

### Retired

`phrases.json` is replaced by the packs:

- Delete `config/factions/*/phrases.json` and the fixtures' copies.
- Delete `FactionFlavorConfig::phrases`, `FactionConfigParser::ParsePhrases` and
  `k_PhrasesFile`.
- Delete `FactionFlavor::PickPhrase` and `Format`. `PickPhrase` drew from the base-name RNG,
  which is save state.

## Script markup

`include/ui/text/ScriptMarkup.h`:

```cpp
struct ScriptValue_t
{
    std::string text;
    GrammaticalGender_t gender = GrammaticalGender_t::Neuter;
    bool bPlural = false;
};

// A slot's value, as SMAC's parse_says / parse_num leave it.
struct ScriptArgs_t
{
    std::map<int, ScriptValue_t> strings;
    std::map<int, int> numbers;
};

struct TextRun_t
{
    std::string text;
    bool bHighlight = false;
};

// SMAC's script markup (terranx-map.md, parse_string): slots, gender and number forms, and
// highlighted spans.
std::vector<TextRun_t> FormatScript(const std::string& rTemplate, const ScriptArgs_t& rArgs);

// Every string and number slot rTemplate reads, so a flow can be checked at load.
struct ScriptSlots_t
{
    std::set<int> strings;
    std::set<int> numbers;
};
ScriptSlots_t SlotsOf(const std::string& rTemplate);
```

`FormatScript` implements the grammar the RE confirms, and nothing else. A malformed template
or an unbound slot throws.

## Talk script

### talk.json

`config/ui/talk.json` and `tests/fixtures/ui/talk.json`. An excerpt of the shipping file (slot
numbers are SMAC's, from the bindings recorded in the map):

```json
{
  "bindings": {
    "introduce": {
      "0": "player.title_leader", "1": "player.name_leader",
      "2": "rival.title_leader", "3": "rival.name_leader", "4": "rival.noun_faction",
      "6": "rival.phrase.intent", "8": "rival.adj_name_faction", "9": "player.adj_leader"
    },
    "menu": { "0": "player.name_leader", "1": "pact_hood" },
    "broke_pact": {
      "0": "player.pact_brother", "1": "player.title_leader", "2": "player.name_leader"
    },
    "credits": { "#0": "candidate.amount" }
  },
  "start": {
    "say": [
      { "text": "INTRONEW0", "args": "introduce" },
      { "text": "INTRONEW2", "args": "introduce", "extra": { "5": "player.noun_faction" } }
    ],
    "then": "diplomacy"
  },
  "scenes": {
    "confirm_renounce": {
      "prompt": { "text": "REALLYBROKE", "args": "menu" },
      "choices": [
        { "label": { "text": "REALLYBROKE", "item": 0, "args": "menu" },
          "say": { "text": "DROIDS", "args": "droids" }, "then": "diplomacy" },
        { "label": { "text": "REALLYBROKE", "item": 1, "args": "menu" }, "do": "RenouncePact",
          "say": { "text": "BROKEPACT", "args": "broke_pact" }, "then": "diplomacy" }
      ]
    },
    "trade": {
      "prompt": { "text": "TALKTRADEPROMPT", "args": "talk" },
      "draft": { "offer": { "text": "TALKOFFERING" }, "ask": { "text": "TALKASKING" } },
      "choices": [
        { "from": "trade_kinds", "side": "Ask",
          "labels": { "Technology": { "text": "PROPOSALMENU", "item": 4, "args": "proposal" } },
          "then": "pick" },
        { "label": { "text": "TALKPROPOSETRADE" }, "do": "ProposeTrade",
          "on": { "Accepted": { "say": { "text": "TALKACCEPTTRADE", "args": "talk" }, "then": "diplomacy" },
                  "Rejected": { "say": { "text": "TALKREFUSETRADE", "args": "talk" }, "then": "diplomacy" },
                  "Invalid": { "say": { "text": "TALKINVALID", "args": "talk" }, "then": "trade" } } }
      ]
    }
  }
}
```

- **`bindings`**: named slot sets, the analog of the `parse_says` block before each `X_pops`.
  Keys `"n"` are string slots and `"#n"` number slots. A text reference names one in `args`,
  and `extra` adds slots for that reference alone. `REALLYBROKE` keeps the menu's slots, as
  `communicate` does.
- **`portrait`**: the labels `TalkPopup` draws, `{ "mood": 933, "reputation": 932,
  "integrity": 222 }`.
- **`credit_offers`**: the amounts a credits trade row lists.
- **`say` and `prompt`** are text references. A list of them is picked at random, like SMAC's
  `random(n)` suffix. `INTRONEW3` stays out until a ranking exists (Deviations).
- **A choice** is one of two things:
  - a `label` (a text reference with `item`), plus optional `when`, `do`, `say`, `then` and `on`;
  - a generated row: `from` names a generator, `labels` gives a text reference per generated
    kind, and `then` says where to go.
- **Results.** `then` alone sends every result of `do` there. Otherwise `on` must give each
  result of `do` its own `say` and `then`.
- **`end`** closes the talk.

The shipping file covers:

- start;
- diplomacy (proposal, renounce, finished);
- confirm_renounce;
- proposal: Truce on `PROPOSALMENU` item 3 at Vendetta, Treaty on item 3 otherwise, Pact on
  item 2, Trade, never mind;
- trade: offer and ask rows for each kind, propose, clear, never mind;
- pick.

Accept lines are `MAKETRUCE`, `MAKETREATY0` / `1` and `MAKEPACT`. Refusals are `REJTREATY` and
`REJPACT`, and `TALK*` for trades. The trade-ask labels use SMAC's `PROPOSALMENU` lines
(research data, commlink, energy, survey data, joint attack, cede a base). The offer labels are
`TALK*` ids.

### C++ vocabularies

These are closed sets, each next to its implementation.

| Kind | Names | Backed by |
|---|---|---|
| Conditions (`when`) | `MayProposeTruce`, `MayProposeTreaty`, `MayProposePact`, `MayRenouncePact`, `MayTrade`, `HasDraft` | `GetTalkActions`; the draft |
| Actions (`do`) | `ProposeTruce`, `ProposeTreaty`, `ProposePact`, `ProposeTrade` → `Accepted` / `Rejected` / `Invalid`; `RenouncePact`, `ClearTrade` → `Done` | `Propose`, `CancelTreaty`; each is available while its condition holds (`ProposeTrade` also needs a draft) |
| Generators (`from`) | `trade_kinds` (with `side` `Offer` / `Ask`), `trade_candidates` | `TradeKinds()`, then candidates filtered by `CanDeliver` |
| Value sources | `player.` / `rival.` + a vocabulary field, `phrase.<name>`, `noun_phrase.<name>`, `pact_brother`; `pact_hood`; `caption`; `candidate.name`, `candidate.amount` | `TextPack` vocabulary and labels, `StandingCaptions`, trade rows |

Enum names in the file are the enumerators', through `magic_enum`.

- A choice whose `do` is unavailable is not shown. `when` hides a navigation choice the same
  way.
- A proposal result of `PendingPlayer` or `Busy` throws `std::logic_error`, since the recipient
  is an AI.
- `caption` is the rival's `noun_faction`, `" - "`, and
  `CaptionMood(GetFriction(rival, player), status)`, as `diplomacy_caption` builds it.

Candidates by kind:

- **Credits:** each `credit_offers` amount.
- **Technology:** the giver's techs.
- **Base:** the giver's bases.
- **Comm frequency and Declare Vendetta:** every other faction.
- **World map:** the one item.

### TalkScript

`include/ui/commlinks/TalkScript.h` is the parsed `talk.json`. It is loaded with
`TalkScript::Load(path, const TextPack&, const FactionRegistry&)` and rejects:

- unknown keys, a missing `start`, and a `then` naming no scene;
- a text id or `portrait` label no pack root has, or an `item` past the entry's items;
- an unknown action, generator, value source or binding set;
- a slot that a referenced template reads (`SlotsOf`) with no binding, or a number slot bound
  to a string source;
- a `do` whose results are not all covered;
- a `trade_kinds` row whose `labels` miss a `TradeKinds()` kind;
- a faction with diplomacy (`IsNativeLifeFaction` false) with no vocabulary file, or one
  missing a field or phrase the bindings read.

## Vendetta caption floor

`config/ui/standing_captions.json` and `tests/fixtures/standing_captions.json` gain
`"vendetta_mood_floor": 15`. [StandingCaptions](include/ui/StandingCaptions.h):

```cpp
// The talk caption's mood: SMAC's diplomacy_caption reads friction below the floor as the
// floor at Vendetta without changing it.
const std::string& CaptionMood(int friction, DiplomaticStatus_t status) const;
```

`Load` requires the floor to be an integer in `[0, frictionMax]`.

## Talk UI

- **`TalkBindings`** (`ui/commlinks/`) resolves a binding set to `ScriptArgs_t` for one player
  and rival.
- **`TalkConversation`** (`ui/commlinks/`) interprets the `TalkScript` for one rival. It holds:
  - the scene;
  - the rival's last line as runs;
  - the trade draft (`give` and `ask` `TradeItem_t` lists);
  - a `std::mt19937` for variant picks.

  It builds the scene's prompt and choices as `PopupChoice_t`, and runs actions. A choice only
  records the next scene.
- **`WrapText`** (`include/ui/TextWrap.h`) wraps `TextRun_t` lines to a width with
  `MeasureTextWidth`. It keeps the markup's line breaks, and a word wider than the width gets a
  line to itself.
- **`TalkPopup`** (`ui/commlinks/`) is modal and owns the conversation and a `ListSelectorPopup`
  menu (prompt as the title, run text flattened). `Render` draws:
  - the frame;
  - the caption (`rival.caption`);
  - the `portrait` mood label and the raw `Mood(GetFriction(rival, player))`;
  - the reputation label and `Repute(GetBlemishes(rival))`;
  - the integrity label and `Repute(GetBlemishes(player))`;
  - the rival's line wrapped, with highlighted runs in `highlight_color`;
  - in the trade scene, the draft's items;
  - the menu.

  Input goes to the menu. After the forwarded key or click returns, `TalkPopup` builds the next
  menu, or closes on `end` or on a dismiss (the menu closed with no choice). It never replaces
  the menu inside a choice callback, because `ListSelectorPopup` runs `onChosen` from its own
  `HandleMouseClick`.
- **`CommlinksPanel`** takes `std::function<void(FactionId_t)> onTalk`. `MetFactions_` gives the
  rows in draw order, and `HandleMouseClick` calls `onTalk` with the row under the press.
  Native life never has a row.
- **Wiring**:
  - `Engine::InitializeUi_` loads `TextPack` and then `TalkScript` beside `StandingCaptions`.
  - `ViewFactory`, `CommlinksView` and `TalkPopup` take them.
  - `CommlinksView::OpenTalk_` runs `DismissOpenModals_()`, then pushes a `TalkPopup` at
    `GetPopupLayout()` seeded with `std::random_device{}()`.
  - `TestHelpers` and `ViewFixture` load the fixture pack and script.
- **Style**: `TalkPopupStyle_t`, required by `ParseTalkPopupStyle_`, under `talk_popup` in both
  `style.json` files. Run `python tools/format_ui_style_json.py` afterwards.

  | Key | Shipping |
  |---|---|
  | `background_color`, `border_color` | as `commlinks_panel` |
  | `caption_color`, `label_color`, `text_color`, `highlight_color` | 255,255,0 / 160,160,200 / 255,255,255 / 255,255,0 |
  | `caption_font_size`, `text_font_size` | 18, 16 |
  | `pad_x` | 0.05 |
  | `value_pad_x` | 0.3 |
  | `caption_pad_y`, `mood_pad_y`, `repute_pad_y`, `integrity_pad_y` | 0.03, 0.09, 0.13, 0.17 |
  | `speech_pad_y`, `line_height`, `draft_pad_y` | 0.24, 0.04, 0.42 |
  | `menu_layout` | 0.05, 0.56, 0.9, 0.42 |

## Deviations

New TODO entries in [config/deviations/README.md](config/deviations/README.md):

- **Trade proposals bundle items `[Ours]`.**
  - **Original:** each `proposal_menu` line asks for or offers one thing, and the AI names a
    price or counter-offer (`COUNTER0`, `COUNTERMENU`).
  - **Ours:** the player builds both sides of one proposal, and the AI accepts.
  - **Where:** `talk.json` trade scenes, `TalkConversation`.
  - **Missing:** the one-item flow with AI counter-offers, and a switch. Once AI evaluation
    exists, the switch can be a variant `talk.json` in `config/deviations/`.
- **Talk gifts and trades write no standing.**
  - **Original:** `communicate`'s gift and trade branches move friction (−1 / −2 / +1 and
    computed gift deltas) and `diplo_gifts`.
  - **Ours:** `ApplyProposal` moves the items only. Leave a TODO there.
  - **Where:** `ApplyProposal`.
  - **Missing:** the RE of those deltas, a gifts record, and the writers.
- **Ranked greeting.**
  - **Original:** `INTRONEW3` names the player's empire by a rank word from `7 - ranking`.
  - **Ours:** there is no faction ranking, so `talk.json` leaves `INTRONEW3` out.
  - **Where:** `talk.json` `start`.
  - **Missing:** a ranking, its value source, and the variant.
- **Talk proposes a Truce at Vendetta `[Ours]`.** SMAC's Treaty item at Vendetta makes a Treaty
  through `propose_treaty` / `make_treaty`; Blood Truce comes only from an AI's offer. Ours
  follows `StepUp(Vendetta)`. Missing: the original transition and a switch.
- **Submissive caption.** SMAC can show label 488 `Submissive` in place of the mood; there is
  no surrender state. Missing: what `0x93FA98` holds, a surrender state, and the label.
- **Treaty acceptance caption `[Bug]`.** `make_treaty` overwrites slot 7, so SMAC's `MAKETREATY`
  caption shows a pet project. Ours draws the caption binding. Missing: the original version
  and a switch.

## Docs

[ui-system.md](docs/architecture/ui-system.md):

- A "Text packs" section: roots and overlay, locale, the three files, `extract_text.py`, the
  in-repo English pack, `ScriptMarkup`.
- A "Talk" section: the three layers, `TalkScript` validation, the vocabularies,
  `TalkConversation`, `TalkPopup`.
- The Commlinks rows open talk.
- `StandingCaptions` gains `CaptionMood`.

[diplomacy-system.md](docs/architecture/diplomacy-system.md):

- Diagram:
  - `GetTalkActions` and `CanDeliver` nodes.
  - A talk UI node with edges to `GetTalkActions`, `Propose`, `CancelTreaty` and `CanDeliver`.
- Renouncing a Pact in talk writes the agreement-break blemish, as SMAC's inline `+ 1` does.
- `DeclareVendetta`'s callers are trade and the border break.
- "Where the rules live" gains `GetTalkActions` and `CanDeliver` rows.
- "Not yet built": the returning and hostile greetings wait on the introduction and revenge
  records.

[graphics_assets_roadmap_04a90a53.plan.md](docs/plans/graphics_assets_roadmap_04a90a53.plan.md)'s
extractor table gains `extract_text.py` → `assets/text/<locale>/…`.

[refactor-flags.md](docs/plans/refactor-flags.md):

- Drop `PickPhrase`.
- `Propose`, `GetDiplomaticActionExecutor`, `GetAvailableActions` and `TradeKinds` gain a
  caller.

## Tests

The fixtures own the script, the pack, the vocabulary and the numbers. No shipping-config or
shipping-pack asserts.

- **`ScriptMarkup`:**
  - string and number slots;
  - each form selector against gender and plural;
  - highlighted runs;
  - `SlotsOf`;
  - an unbound slot and a malformed template throw.
- **`TextPack`** (two fixture roots):
  - An entry in both roots resolves from the first.
  - An entry in only the second resolves.
  - A faction file resolves whole from one root.
  - A missing root is skipped.
  - An unknown id throws.
  - `items` follow the blank-line rule.
- **`TalkScript`:** one rejection case per bullet under TalkScript, each against a temp variant
  of the fixture.
- **`extract_text.py`**: `tests/tools/test_extract_text.py` (`unittest`) runs over a small
  synthetic install under `tests/fixtures/smac_text/`. It is registered with `add_test` in
  `tests/CMakeLists.txt`, so `./bd test` runs it. It covers:
  - labels, directives, body and items;
  - the faction header fields and named, tagged phrases, and a file without 15 phrase lines;
  - the `gaian` id mapping;
  - cp1252 input and file names in any case.
- **`StandingCaptions`:** `CaptionMood` below, at and above the floor, and outside Vendetta.
  `Load` rejects a missing, negative or above-maximum floor.
- **`GetTalkActions`:**
  - At Pact: includes `CancelTreaty`.
  - At Truce and Treaty: no `CancelTreaty`.
  - Never `DeclareVendetta`.
  - StepUp and Trade as `GetAvailableActions`.
  - Unmet pair: empty.
- **`CanDeliver`:** one deliverable and one undeliverable item per kind.
- **`CommlinksPanel`:** a click on a met row calls `onTalk` with its id; a click below the rows
  does not.
- **`TalkPopup`** (`ViewFixture`, `RecordingGraphics`, fixture script and pack):
  - The opening draws the caption with the Vendetta floor, the raw mood, the repute, the
    integrity line and a start line with its slots filled.
  - Proposing the offered StepUp sets the status and plays its accept text.
  - Renounce shows only at Pact. Backing down keeps the Pact. Renouncing leaves a Treaty,
    blemishes the player by the fixture's `break_agreement_blemishes`, and plays the renounced
    text.
  - A traded tech moves and plays the trade accept text.
  - Escape and finished close the popup and change nothing.
- Remove the `phrases` asserts in `FactionConfigParserTests` and the `PickPhrase` / `Format`
  cases in `FactionFlavorTests`. The requirement moved to the text packs.

## Verification

1. `./bd test`, and confirm in `LastTest.log` that the new cases ran, the extractor test
   included.
2. `./bd build`, then screenshot the greeting, the proposal menu and the trade scene from the
   in-repo pack alone.
3. Run `python extract_text.py`, and screenshot the same scenes again with SMAC's text.
4. Check that the caption, standing lines, wrapped speech and menu fit in both.
