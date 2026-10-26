# terranx.exe map

What we have confirmed in the original binary (GOG Alien Crossfire v2.0, SHA-1
`4b19c1fe3266b5ebc4305cd182ed6e864e3a1c4a`, image base `0x400000`), with addresses. Names are
Thinker's (`refs/thinker/src/engine.cpp`). Setup and rules are in
`.devin/rules/smac-reverse-engineering.mdc`. Every entry here was read in the disassembly, not
taken from Thinker; where Thinker behaves differently, the entry says so.

## Finding things

| Section | File offset | Address |
|---------|-------------|---------|
| `.text` | `0x001000` | `0x401000` |
| `.rdata` | `0x269000` | `0x669000` |
| `.data` | `0x282000` | `0x682000` |

A string at file offset X is pushed as `push X + 0x400000`. Script labels such as
`ABANDONBASE` are `.data` strings, so search the file for the label, then grep the function's
disassembly for its address.

## Globals and offsets

| Address / offset | Meaning |
|------------------|---------|
| `0x90EA30` | `CurrentBase` (pointer to the `BASE` being processed) |
| `0x689370` | `CurrentBaseID` |
| `0x9A64C0` | `GameState` flags |
| `0x9A64C4` | `DiffLevel` |
| `0x9A64E8` | `FactionStatus` (human-player bitmask) |
| `0x96C9E0` | `Factions`; `diff_level` at +0x8 |
| `0x97D040` | `Bases` |
| `0x9AB868` | `Units` (designs), stride `0x34`; `plan` at +0x2A, `PLAN_COLONY` = 8 |
| `BASE` +0x6 | `pop_size` (byte) |
| `BASE` +0x3C | `nutrients_accumulated` |

## Base turn

### `base_upkeep` (`0x4F79C0`)

Calls, in order: `base_nutrient`, `base_minerals`, `base_energy`, then **`base_production`**
(`0x4F7A2F`), `base_hurry` (`0x4F7A34`), then **`base_growth`** (`0x4F7A52`), then
`base_check_support` and the recompute. Production always runs before growth. Nothing grows a
base early.

### `base_production` (`0x4F07E0`)

**This turn's minerals** (`0x4F08EC`–`0x4F0907`): if `mineral_surplus` (`BASE`+0xD4) ≥ 0, the
queue is not Stockpile Energy (`queue_items[0]`+0x50 ≠ −69), and the base is not rioting
(`state_flags`+0x30 bit 1), add the surplus into `minerals_accumulated` (`BASE`+0x40). This
runs before any completion or "already built" check.

**Secret project already completed** (`0x4F0A39`): after that add, a queued project whose
`SecretProjects[id]` (`0x9A6514`) is not `SP_Unbuilt` (−1) jumps to `0x4F1A93`. The item is
not completed and the queue is not changed. A human (unless the governor is managing
production) is shown `ALREADYPROJECT` / `ALREADYPROJECT1` / `ALREADYPROJECT2` via `popb`
(`0x48C650`) with `WARN_STOP_BUILD_OUT_OF_DATE` (`0x200`) at `0x4F1B95`. `popb` always offers
Continue and Zoom to Base Control; Continue does not pick a new item. AI / governor calls
`base_reset` (`0x4E5FE0`) at `0x4F1BA7` instead of the popup. Thinker's `mod_base_production`
follows the same add-then-notice order.

**Colony pod at size 1** (`0x4F0BF6`): if the item's `plan` is colony, `pop_size == 1` and
the faction's `diff_level > 1`, completion is gated. The gate reads the current size only. It
does not predict growth or starvation this turn.

- AI: does not complete. If `DiffLevel > 3` it adds `DiffLevel − 2` to
  `nutrients_accumulated` and returns (`0x4F0C2E`).
- Human: shows the `ABANDONBASE` popup. All three popup sites (`0x4F0DEE`, `0x4F0EB9`,
  `0x4F0ED8`) push the `ABANDONBASE` string; there is no `ABANDONBASE1` in the binary. An
  answer of "no" leaves the item incomplete this turn.
- Thinker: shows `ABANDONBASE1` (turns until growth) when surplus is positive, marked `Fix:`.

**Pod pop cost** (`0x4F13F8`–`0x4F1466`), after the unit is created: clamps a negative
`nutrients_accumulated` to 0. Then it decrements `pop_size` (`0x4F1441`) unless the base is
size 1 and either `diff_level ≤ 1` or `is_objective` (`0x5AC060`) is true. If `pop_size ≤ 0`
it calls `base_kill` (`0x4E5250`). This happens inside production, before `base_growth`, so
a size-1 base whose tanks are full is still lost on a "yes".

**Facility completion** sets the facility bit inline through `bitmask` (`0x50BA00`) and
`or [eax], dl` (`0x4F21C0`), not through `set_fac`. The facility exists before `base_growth`
runs.

**Spawn morale** (`0x4F12EA`–`0x4F13C5`), after `veh_init`: non-native units start from
`morale_mod` (`0x4E6400`). Probe weapon `0x15` then adds Covert Ops Center +2 and Nethack
Terminus +1. `has_abil` (`0x5BF1F0`) of `ABL_TRAINED` (`0x4000`, High Morale) increments
the same accumulator, then prototype does, then a final +1. The result is written to
`VEH.morale` (`0x95284c`). High Morale is not a live combat modifier.

### `base_growth` (`0x4EF1C0`)

At the population limit it checks Hab Complex and Habitation Dome directly with `has_fac`
(`0x421670`; facility 24 at `0x4EF448`, 25 at `0x4EF479`). A Hab Complex or Dome finished in
`base_production` the same turn therefore lets the base grow that turn.

## Unit morale

### `morale_veh` (`0x5C0E40`)

Does not add High Morale. Combat units are `VEH.morale` plus SE Morale / `rule_morale` /
Creche / Brood Pit / drone riot, clamped 0–6.

Probe plan (`UNIT.plan` `0xb` at `0x9ab892`): SE Probe (clamped 0–3) + Telepathic Matrix +2
+ one per `TFLAG_IMPROVE_PROBE` tech + `VEH.morale`, then **clamp 2–6**. The Disciplined
floor is this live clamp, not a stored spawn of 2. Thinker's `mod_morale_veh` follows the
same split; it does not add `ABL_TRAINED` either.

Combat units (not probe, not basic-psi `unit_id < 0x40 && offense < 0`): clamp SE −4..4,
then +1 if ≤−2 / −1 if ≥2, add negative `rule_morale`. If **home** has facility 2
(Children's Crèche) and the modifier is negative, signed `/2` toward 0 (`cdq; sub; sar 1`
at `0x5C1042`). Basic psi returns `VEH.morale` at `0x5C0F7A` and never sees home Crèche.
Brood Pit home (facility `0x23` at `0x5C1053`) is the same `/2` but only for native-psi
that reach that site — effectively Isle of the Deep (`unit_id == 15`). Thinker's
`mod_morale_veh` removes the home Crèche half (`Fix:`).

### `get_basic_offense` (`0x5015B0`) / `get_basic_defense` (`0x501940`)

`base_at` (`0x4E3A50`) on **that unit's tile**. Facility bits via `bitmask` (`0x50BA00`).

If facility 2: `morale++`, then `morale -= adjusted_SE`. Attack (`0x50168C`) clamps the
rating −4..4 and +1 if ≤−2. Defense (`0x501A14`) zeros positives first, then the same
≤−2 bump. Applies to every unit on the tile, including natives. Thinker's combat path
skips natives and adds HQ +1 (`Fix:`).

Brood Pit (`0x23`): native psi only. After Crèche, +1 and no second subtract
(`0x50171C` / `0x501A9D`). Without Crèche, +1 and rating-subtract; defense does **not**
zero positives (`0x501B15`).

After extras: defense always `clamp(morale, 1, 6)` at `0x501C03`. Attack clamps
1–6 only when `ebp+0x18` (`unk_tgl`, artillery / attacker–defender swap) is set
(`0x501874`–`0x501895`); otherwise the integer is **unbounded**. Both then
`add ebx, 6` and multiply proto by that (`offense_proto` `0x5C1150` at
`0x5018AE`, then `imul eax, ebx` at `0x501929`; defense `0x501C22`). There is
no off-table rank row — strength is `proto * (morale + 6)`. Disciplined (2) is
`* 8` (100%); each step is another 1/8 of proto (+12.5%). Abilities are
`has_abil` (`0x5BF1F0`) after that factor.

Both then add +1 if `SE_morale_pending` (`Faction` +0x25C = `0x96CC3C`) is 2 or 3.
Defense does this whenever `base_at` succeeded (`0x501B4C`). Attack does it only
when `unk_tgl` is set (`0x5017BE`) — Thinker notes that flag is the
attacker/defender swap in `battle_compute`. `SE_morale` (active, +0x288 =
`0x96CC68`) is the value the Crèche/Brood subtract uses; the +2/+3 extra reads
pending.

### `say_morale` (`0x4B3FD0`) / `say_morale_2` (`0x4B43C0`)

Unit-info rank string. `say_morale_2` is a wrapper onto the global buffer at
`0x9B86A0`. The rank **word** is `morale_veh(veh, 0, arg3)` at `0x4B3FE3` —
riot flag off — then the name table at `0x952328` (conventional) /
`0x95232C` (native lifecycle). Riot is not folded into that word.

Annotations after the word, in order:

- Home drone-riot (`BASE` +0x70 bit 1) and `morale_veh > 0` and no
  `RFLAG_MORALE`: append `" (-)"` (`0x6874C8` at `0x4B40C9`).
- On a base and rank &lt; 6: count in-square extras into `0x8C6B54` the same way
  combat does (facility 2, then Brood Pit `0x23` for native psi). Each extra is
  a `"+"` (`0x6874D0`).
- `SE_morale_pending` 2 or 3: one more `"+"` (`0x4B430A`). No on-base gate —
  the mark shows in the field; combat only pays it in a base when defending.
- Rank 0 and no pluses: force one `"+"`.
- Designated defender (`VEH` +0x2C bit 7): append `"(d)"` (`0x6874D4`).

Thinker's `mod_say_morale2` also adds HQ +1 to the plus count (`Fix:`). The
binary does not.

## Sea bases

### `base_init` (`0x4E4B80`)

A base founded on ocean (`MAP.climate & 0xE0 < 0x60`, altitude below 3, at `0x4E5017`) gets
facility `0x1A` (Pressure Dome). The bit is set inline through `bitmask` (`0x50BA00`) and an
`or` into `BASE` +0x8C (`0x4E5034`), not through `set_fac`. There is no separate "sea base"
flag: the Pressure Dome is what lets a base stand on water. Thinker's `mod_base_init` does the
same with `set_fac`.

### `crop_yield` (`0x4E6E50`), `mine_yield` (`0x4E7310`), `energy_yield` (`0x4E7750`)

On a base square, each tests facility 3 (Recycling Tanks), then facility `0x1A` (Pressure
Dome), and adds the `recycling_tanks` bonus once if either is built: nutrients `0x945F90`
(`0x4E6FB1`), minerals `0x945F94` (`0x4E747E`), energy `0x945F98` (`0x4E78F2`). The two do not
stack. A sea base's free dome therefore yields as Recycling Tanks. Thinker's `mod_crop_yield`
and its siblings do the same.

### `alt_set` (`0x591290`)

Runs only when the tile crosses the shore line (`(old < 3) != (new < 3)`, `0x5912FB`). Then:

1. Clears the land-only items (`and 0x7E1377A3`, `0x591320`) for every faction's view.
2. If `base_at` (`0x4E3A50`) finds a base:
   - Tile became land (`test ebx, ebx` at `0x59138D`): nothing happens to the base, and the unit
     pass is skipped. A sea base raised onto land keeps its Pressure Dome.
   - Tile became water and the base has facility `0x1A`: nothing happens, and the unit pass is
     skipped.
   - Otherwise the base is flooded. Population drops by `clamp(pop / 2, game_rand() % 3 + 2,
     10)` (`0x5913D1`–`0x591408`). The base is granted the Pressure Dome (`set_fac(0x1A, base,
     1)` at `0x591415`). The faction's `player_flags |= 0x80000`, and its submerged count
     (`0x9B2290`) goes up. An objective base (`is_objective`, `0x5AC060`) keeps at least 1
     pop.
   - If pop is still above 0, the base survives and the unit pass is skipped (`jg 0x591852` at `0x591488`).
   - At 0 pop, the faction's last base (`base_count` `<= 1`, at `0x59148E`) is kept at pop 1
     with popup `BASESUBMERGED` / `BASESUBMERGED2`. In multiplayer outside the controlling
     turn, it is kept as well. Any other base is shown with `BASESUBMERGED1` / `BASESUBMERGED3`
     and killed (`base_kill` `0x4E5250` at `0x5915A6`).
3. Unit pass (no base, or the base was killed): every unit on the tile that is not air (triad 2),
   not an objective (`VEH` +0x20 bit `0x20`), and whose `triad == sea` differs from the new
   surface is killed (`veh_kill` `0x5C08C0` at `0x591639`). The nearest base is recorded first
   (`base_find_2`, into `0x9B22BC`).

Thinker's `alt_set` clamps the reduced pop at 0 before testing it (`Fix:`, the original can
store a negative byte). Otherwise it follows the same flow.

## Research

### `tech_rate` (`0x5BE6B0`)

Faction-wide research cost for the current tech (same number for every tech at that moment).
`tech_research` follows at `0x5BE940`. If the cached `Faction.tech_cost` (`+0x3D0` / `0x96CDB0`)
is ≥ 0, that value is returned. If `Rules.tech_discovery_rate` (`0x949758`) is 0, the cost is
`999999999`.

`tech_rate` never writes the cache. The only writers are `setup_player` (`0x5B13FB`, −1) and
`tech_research` straight after a breakthrough (`0x5BEABD` −1, then `0x5BEACF` stores a fresh
`tech_rate`). So a faction's first tech is priced live (turn, MOSTTECHS, TECHS); after that the
cost is fixed at each breakthrough, and techs gained by trade or probe do not reprice it.
The same block zeroes `tech_accumulated` (`+0x3C8`): surplus research is lost.

**TECHS** (`0x5BE6F1`–`0x5BE774`):
`clamp(tech_ranking + 2 * earned_techs_saved − unk_27, 2, 9999) / 2`.
`tech_ranking` (`+0x35C`) is twice the discovered-tech count, but `tech_achieved`
(`0x5BB000`; guard `0x5BB482`–`0x5BB49D`) skips it while `CurrentTurn` is 0 or `SkipTechScreenA` (`0x945F40`) is set, so
starting techs and the turn-1 free techs (`rule_tech_selected`) are not counted.
`earned_techs_saved` (`+0x3D4`)
is the wiki's varA (deferred MP / PBEM discoveries). `unk_27` (`+0x360`) is the wiki's varB,
in half-tech units.

**MOSTTECHS** (`0x5BE72C`–`0x5BE75E`): max of `tech_ranking + 2 * earned_techs_saved` over
`Factions[1]`…`Factions[7]` (loop `0x96EE08`–`0x97D39C`, stride `0x20CC`), then `/ 2`.
Faction 0 is skipped. `unk_27` is not subtracted.

**Difficulty** (`0x5BE761`–`0x5BE7B7`). Human bit is `1 << faction_id` against
`FactionStatus` (`0x9A64E8`). Raw `DiffLevel` (`0x9A64C4`) is 0–5 (Citizen…Transcend).
`diff_adj = DiffLevel + (DiffLevel < 3)` (Talent and Librarian both become 3). Humans read
`Faction.diff_level` (`+0x8`); AIs read the game `DiffLevel`. Factor is
`4 * diff_adj + 8` (human) or `29 − 3 * diff_adj` (AI), then
`clamp(factor, 12 − TECHS, 12 + TECHS)` (`0x5BE7B9`–`0x5BE7D5`) — not ±12.

**Turn lag** (`0x5BE7D5`–`0x5BE80D`): `TECHS − CurrentTurn / (stagnation ? 12 : 8)`.
`CurrentTurn` (`0x9A64D4`) is 0 during setup and 1 on the first playable turn; `turn_upkeep`
increments it (`0x525ADC`). Then
clamped to `[0, factor * (stagnation ? 3 : 2) / 2]`. Stagnation is `GameRules` (`0x9A649C`)
bit `0x20`. Added onto the factor.

**Catch-up** (`0x5BE80F`–`0x5BE855`). Uses raw `DiffLevel` for AIs, **hardcoded 3** for
humans — not `diff_adj`:

| | divisor | max deduction |
|---|---|---|
| Human | 5 | `3 * factor / 10 + 1` (30% + 1) |
| AI Citizen…Transcend (`DiffLevel` 0–5) | `8 − DiffLevel` (8, 7, 6, 5, 4, 3) | `DiffLevel * factor / 10 + 1` (0, 10, 20, 30, 40, 50% + 1) |

Amount is `ceil((MOSTTECHS − TECHS) / divisor)` via
`(MOSTTECHS − diff_val − TECHS + 7) / (8 − diff_val)`, then clamped to `[0, max]`.

**Tech count factor** (`0x5BE857`–`0x5BE88F`):
`SE_research_base` (`+0x96CCD8` on the faction pointer) is sign-clamped
(`> 0` → 1, `< 0` → −1, else 0). Then
`clamp(TECHS − that, 1, 99999)` (`0x1869F`) and `imul` by `(factor − catch_up)`.
Not a percent of cost. Ratings past ±1 do not change this term.

**Modifiers**, in this order (`0x5BE892`–`0x5BE907`): `* 100 / tech_discovery_rate` when
rate ≠ 100; `* rule_techcost / 100` when ≠ 100 (`MFactions` `0x946F48`);
`* MapAreaSqRoot / 56` (`0x949888`, magic `0x92492493` then `sar 5`). `MapAreaSqRoot` is
`quick_root(MapAreaY * (MapAreaX / 2))` (`0x590F4F`–`0x590F62`; `quick_root` `0x6290E0` is a
floor integer square root), the root of the real tile count: 56 on a standard map.
`psych_check` (`0x4EA4A0`) scales the bureaucracy limit by the same `/ 56`;
`+ cost / 2` if stagnation. Final clamp `[1, 99999999]`. No per-tech cost term.

Thinker's `mod_tech_rate` (`refs/thinker/src/tech.cpp`) can replace the whole function
(`revised_tech_cost`). Its reconstruction of this path includes faction 0 in MOSTTECHS
and uses `int64_t` to avoid overflow; the original is 32-bit. Shipping
`config/tech_cost.lua` matches this function through the map step
(`WorldMap::GetAreaSqRoot`, turn from `GameState::GetTurnNumber`), takes MOSTTECHS
over every faction in the current session, and uses `max(1, discovered)` for TECHS
(no 9999 ranking cap). The discovery rate, `rule_techcost` and ThisTech are the
`TechCost` product after the formula. Stagnation's 1.5× and turn lag are `TechCost` /
`TechCostTurnLag` from `config/world_rules.json`. Inherent `SOCIAL RESEARCH`
is `TechCostResearch` (shipping ±1; Lua subtracts it and floors the local
tech factor at 1). Live Research SE scales labs.
Thinker's per-tech replacement is `tech_alt_cost`, recorded in
`config/research/manual/tech_cost_thinker.lua` (not on `GameDataPaths`).

University (`UNIV.TXT`) and Cybernetic Consciousness (`cyborg.txt`) do not set
`TECHCOST` / `rule_techcost`. Both are `SOCIAL, ++RESEARCH`.
`compute_faction_modifiers` (`0x5B0D70`) zeros the 11 `SE_*_base` dwords
(`0x96CCB0`) and, for each `RULE_SOCIAL` (id 3) bonus, adds the signed value
into that vector. Research is index 10, so `++RESEARCH` lands on
`SE_research_base` (`0x96CCD8`). `social_calc` (`0x5B4210`) then adds the
whole `SE_*_base` block into the live rating. That is the labs-scale in
`base_research` plus the sign-clamped ±1 TECHS term above — not an 80% cost
multiplier.

### `base_energy` labs (`0x4EB560`)

`labs_total` (`BASE +0x108`) is this base's labs output. Facilities never enter
`tech_rate`. After the energy-slider split (`0x4EBA54`) and specialists:

- `MFaction.rule_research` (`0x946F2C`, "free labs per base") is added (`0x4EBC37`).
- Biology Lab (facility 9) adds **+2** (`0x4EBC85`). Thinker uses
  `conf.biology_lab_bonus`.
- Facility coefficients start at 4 (`0x4EBC99`). Each labs building adds +2
  (so +50% after `(coeff * labs + 3) / 4` at `0x4EBFA5`): Network Node (8),
  Research Hospital (17, also +1 psych), Nanohospital (18, also +1 psych),
  Fusion Lab (15, also +2 econ), Quantum Lab (16, also +2 econ).
- Super Collider (`SecretProjects` `0x9A6544`) and Theory of Everything
  (`0x9A655C`) each `shl` labs (`*2`) if this is the host base (`0x4EBFC0`,
  `0x4EBFE7`). They stack.
- Network Backbone (`0x9A6568`): +1 lab per Network Node in the game, plus
  commerce or (aliens) energy-grid (`0x4EC09C`–`0x4EC16B`).

Punishment Sphere is not here; it halves in `base_research`.

### `base_research` (`0x4F4E80`)

Per-base labs contributed this turn, then `tech_research` (`0x5BE940`). Skipped when
`GameRules` (`0x9A649C`) bit `0x4000000` (scenario no tech). Punishment Sphere halves
`labs_total` (`BASE +0x108`). Two start delays zero labs: `CurrentTurn`
(`0x9A64D4`) `< −5 * SE_research_pending` (`+0x96CC54`), and `CurrentTurn < 5` when
`diff_level` (`+0x96C9E8`) `< 2`.

Research SE then scales the contribution (`0x4F4F37`–`0x4F4F9C`), not the energy split
and not tech cost: clamp `SE_research_pending` to `[−5, 5]`,
`10 * labs * (SE + 10) / 100` (integer), remainder carried on `net_random_event`
(`+0x96CDB8`); if that remainder reaches 100, add 100 to the numerator and wrap.
Call is `tech_research(faction, that / 100)`. That is ±10% labs income per Research
level.

Thinker's `mod_base_research` (`refs/thinker/src/base.cpp`) uses the same scale.
`conf.early_research_start` skips both start delays.
