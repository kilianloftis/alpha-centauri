---
name: Diplomatic mood and repute
overview: Caption the stored standing counts in the UI. A new StandingCaptions table, loaded from config/ui/standing_captions.json, maps friction to SMAC's nine #MOOD bands and blemishes to its eight #REPUTE names. The commlinks panel shows each met faction's mood toward the player, its repute, and the player's own integrity. The game layer does not change.
todos:
  - id: re-record
    content: terranx-map.md get_mood / Integrity display — portrait reads raw friction[rival][player], InfoWin's Integrity line is the player's own count, the REPUTATION / MOOD / Integrity labels
    status: completed
  - id: captions
    content: StandingCaptions (ui/) loads config/ui/standing_captions.json + fixture; Mood(friction) / Repute(blemishes) through one band lookup; Load validates against DiplomacyConfig_t maxima
    status: completed
  - id: wiring
    content: GameDataPaths.standingCaptions; Engine owns StandingCaptions beside HotkeyConfig; ViewFactory → CommlinksView → CommlinksPanel take it; TestHelpers / ViewFixture load the fixture
    status: completed
  - id: commlinks-panel
    content: CommlinksPanel draws the player's integrity line, Mood / Reputation column labels, and each met faction's mood toward the player and repute; style keys in shipping + fixture style.json
    status: completed
  - id: docs-tests
    content: ui-system.md and diplomacy-system.md; StandingCaptions and panel tests; ./bd test; screenshot
    status: completed
isProject: false
---

# Diplomatic mood and repute

## Context

The ledger stores directed friction (0–`friction_max`) and per-faction blemishes
(0–`blemishes_max`). `DiplomacyCalculator` clamps them and `DiplomaticStandingMutations` writes
them. Mood and repute are captions over those counts, and they live in the UI. In `terranx.exe`
(`sha1` `4b19c1fe3266b5ebc4305cd182ed6e864e3a1c4a`) every reader of `get_mood` and `#REPUTE` is
draw code, and the AI reads raw friction and `reputation()`. This slice adds the caption table
and the first screen that uses it. The talk portrait will use the same table.

## Confirmed in the binary

Add to [terranx-map.md](docs/thinker/terranx-map.md) under `get_mood` and "Integrity display":

- **Portrait** (`DiploPop_draw_portrait` `0x43F790`): the mood line reads the stored
  `diplo_friction[rival][CurrentPlayerFaction]` (`0x43FA1E`–`0x43FA2C`), which is the rival's
  friction toward the player. It is raw, with no Vendetta floor; only `diplomacy_caption` floors.
  Labels are `REPUTATION` (label 932) beside the rival's `#REPUTE` (`0x43F955`) and `MOOD`
  (label 933, `0x43F9E8`) beside its mood.
- **InfoWin** (`InfoWin_setup_text` `0x456300`): one line type (`0x456D16`) draws the cached
  rival's (`DiploFrictionFactionID` `0x93FAC0`) mood from `DiploFriction`. Another (`0x456F13`)
  draws `CurrentPlayerFaction`'s own title and `Integrity` (label 222) with
  `clamp(integrity_blemishes[player], 0, 7)` (`0x457049`–`0x457064`).
- **Intel report** (`ReportWin_draw_intel` `0x4A2210`): calls `diplomacy_check` (`0x53A980`) at
  `0x4A26CE`, then draws `MOOD` from the cached `DiploFriction` (`0x4A2749`).

## Caption table

New `config/ui/standing_captions.json`, and `tests/fixtures/standing_captions.json` beside the
hotkeys fixture, both with the binary's values:

```json
{
  "moods": [
    { "name": "Magnanimous", "min_friction": 0 },
    { "name": "Solicitous", "min_friction": 1 },
    { "name": "Cooperative", "min_friction": 3 },
    { "name": "Noncommittal", "min_friction": 5 },
    { "name": "Ambivalent", "min_friction": 9 },
    { "name": "Obstinate", "min_friction": 13 },
    { "name": "Quarrelsome", "min_friction": 16 },
    { "name": "Belligerent", "min_friction": 18 },
    { "name": "Seething", "min_friction": 20 }
  ],
  "repute": [
    { "name": "Noble", "min_blemishes": 0 },
    { "name": "Faithful", "min_blemishes": 1 },
    { "name": "Scrupulous", "min_blemishes": 2 },
    { "name": "Dependable", "min_blemishes": 3 },
    { "name": "Ruthless", "min_blemishes": 4 },
    { "name": "Treacherous", "min_blemishes": 5 },
    { "name": "Wicked", "min_blemishes": 6 },
    { "name": "Infamous", "min_blemishes": 7 }
  ]
}
```

New [StandingCaptions.h](include/ui/StandingCaptions.h) / `src/ui/StandingCaptions.cpp` (add to
`src/CMakeLists.txt`), shaped like `HotkeyConfig`:

```cpp
// Mood and repute names for stored friction and blemish counts, from
// config/ui/standing_captions.json.
class StandingCaptions
{
public:
    static StandingCaptions Load(const std::string& rConfigPath,
                                 const DiplomacyConfig_t& rDiplomacy);

    const std::string& Mood(int friction) const;
    const std::string& Repute(int blemishes) const;

private:
    struct Band_t
    {
        std::string name;
        int min = 0;
    };
    ...
    std::vector<Band_t> m_moods;
    std::vector<Band_t> m_repute;
};
```

- `Mood` and `Repute` share one private band lookup: the name of the last band whose
  `min <= value`. Stored counts never go below 0 and the first band starts at 0, so a band always
  matches. A count past the last band's min reads the last band, as SMAC's 0–7 clamp shows 8+
  blemishes as Infamous.
- `Load` rejects unknown top-level and entry keys. It requires both lists, and each must:
  - be non-empty;
  - have entries with exactly `name` (non-empty string) and the min key (integer);
  - start at 0;
  - rise strictly;
  - end at or below `rDiplomacy.frictionMax` / `blemishesMax`, so every band can be reached.

## Wiring

- [GameDataPaths.h](include/game/GameDataPaths.h): `standingCaptions =
  "config/ui/standing_captions.json"`, beside `hotkeys`.
- [Engine.cpp](src/game/Engine.cpp) `InitializeUi_`: `m_pStandingCaptions` is loaded beside
  `m_pHotkeys`, from that path and `*m_gameDataContext->diplomacyConfig`, and passed to
  `ViewFactory`.
- [ViewFactory](include/ui/ViewFactory.h) holds `const StandingCaptions&`.
  `CreateCommlinksView` passes it to `CommlinksView`, which passes it to `CommlinksPanel`.
- [TestHelpers.h](tests/TestHelpers.h) sets `paths.standingCaptions` to the fixture.
  [ViewFixture.h](tests/ViewFixture.h) loads it beside `pHotkeys` and passes it to
  `ViewFactory`.

## Commlinks panel

[CommlinksPanel.cpp](src/ui/commlinks/CommlinksPanel.cpp) reads the ledger as it already does
for status. `Render` draws the chrome and title, then `DrawIntegrity_`, `DrawColumnLabels_`, and
one `DrawRow_` per met faction:

- **Integrity line** under the title: `"Integrity: " + Repute(GetBlemishes(player))` (SMAC's
  InfoWin line).
- **Column labels** `Mood` and `Reputation` over their columns.
- **Row**:
  - name;
  - status when not Neutral (as now);
  - `Mood(GetFriction(other, player))`, the other faction's mood toward the player, raw at
    Vendetta as on the portrait;
  - `Repute(GetBlemishes(other))`.

Standing text uses `status_color` and `row_font_size`. New `CommlinksPanelStyle_t` keys, required
by `ParseCommlinksPanelStyle_`, in [config/ui/style.json](config/ui/style.json) and
[tests/fixtures/ui/style.json](tests/fixtures/ui/style.json):

| Key | Shipping |
|---|---|
| `integrity_pad_y` | 0.11 |
| `column_label_pad_y` | 0.17 |
| `mood_pad_x` | 0.58 |
| `repute_pad_x` | 0.78 |

Move `row_start_y` to 0.23 and `status_pad_x` to 0.40. Seven rows end at 0.79, above the council
button (0.88). Tune the x pads against the longest faction name in the screenshot.

## Docs

[ui-system.md](docs/architecture/ui-system.md):

- Beside the `HotkeyConfig` bullet, add **StandingCaptions**: loaded once from
  `config/ui/standing_captions.json` and checked against `diplomacy.json`'s maxima. It names
  stored friction (mood) and blemishes (repute) by band. `CommlinksPanel` uses it now, and the
  talk portrait will.
- The `ViewFactory` dependencies line adds `StandingCaptions`.

[diplomacy-system.md](docs/architecture/diplomacy-system.md):

- Diagram: a `CommlinksPanel + StandingCaptions (UI)` node with an edge to `DiplomacyLedger`
  (`GetFriction` / `GetBlemishes`).
- "Not yet built", Talk standing: replace "Mood and repute captions are UI over the stored
  integers" with "Mood and repute captions are `StandingCaptions` (UI). The talk caption's
  Vendetta floor (`diplomacy_caption`) lands with talk."

No deviations entry: bands and names match the binary.

## Tests

Bands and names come from the fixture. No shipping-config asserts.

- `StandingCaptions` (new `tests/ui/StandingCaptionsTests.cpp`, fixture plus `TempConfigFile`
  variants):
  - `Mood` at each band edge: 0, 1, 2, 3, 4, 5, 8, 9, 12, 13, 15, 16, 17, 18, 19, 20.
  - `Repute` at 0–7, and 8 and 99 → Infamous.
  - `Load` rejects, for each list:
    - a missing key or an empty list;
    - a first min that is not 0;
    - a min not above the one before;
    - a last min above the fixture diplomacy config's maximum;
    - a missing or empty name, or an unknown key.
- Panel (new `tests/ui/CommlinksPanelTests.cpp`, `ViewFixture` + `RecordingGraphics`):
  - A met faction's row draws its mood toward the player and its repute.
  - Friction from the player toward it does not change the row.
  - At Vendetta with friction 0 the row draws Magnanimous.
  - An unmet faction draws no row.
  - The integrity line draws the player's repute.

## Verification

`./bd test`. Check `LastTest.log` to confirm that the new `StandingCaptions` and commlinks-panel
cases ran. Then `./bd build`, open Commlinks in the game, and screenshot it to check that the
columns fit.
