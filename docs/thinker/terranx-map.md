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
| `BASE` +0x4 | `faction_id` (byte) |
| `BASE` +0x5 | `faction_id_former` (byte) |
| `BASE` +0x6 | `pop_size` (byte) |
| `BASE` +0x7 | `assimilation_turns_left` (byte) |
| `BASE` +0x8 | `nerve_staple_turns_left` (byte) |
| `BASE` +0x30 | `state_flags`; bit 1 (`0x2`) is drone riot |
| `BASE` +0x118 | `talent_total` |
| `BASE` +0x11C | `drone_total` |
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

### Combat odds (`battle_fight_2` `0x506AF0`)

Player confirm lives inside `battle_fight_2`, not `compute_odds` (`0x565F20`).
`compute_odds` is the vanilla air/land AI heuristic (unit cost, `stack_check`,
triad). Thinker does not replace it.

**Firepower** (`0x506F6D`–`0x507007`). After `battle_compute` (`0x501DA0`)
writes 8.8 `offense_out` / `defense_out`:

- Conventional (attacker weapon at `UNIT+0x9ab88d` ≥ 0 **and** defender armor
  at `+0x9ab88e` ≥ 0): 1 damage per lost round (`0x506FBA` / `0x506FF8`).
- Psi (either signed byte `< 0`, `jl` at `0x506F90` / `0x506FB8` /
  `0x506FF6`): the **receiver's** `reactor_id` (`UNIT+0x9ab88f`).

Thinker's `ignore_reactor_power` (default on) `short_jump`s `0x506F90` and
`0x506FF6` (`refs/thinker/src/patch.cpp`), so conventional fights use reactor
firepower too.

**Dialog** (`0x508034`–`0x5082DB`). Artifact (`UNIT.plan == 0xC` at
`0x9ab892`): HP = 1. Else `clamp(reactor_id, 1, 100) * 10 − damage_taken`
(`VEH+0x952838`), clamped `[0, 9999]`. Then `esi = HP_atk * offense_out`
(`0x508093`), `edi = HP_def * defense_out` (`0x5080F3`). Trial divisor from 2
while both divide evenly (`0x5080F7`–`0x508135`), then magic divides when both
exceed 20000 / 10000 / 2000 / 1000 / 200 / 100. `parse_num` (`0x625E30`) at
`0x5082AF` / `0x5082B7`. `#GOODIDEA` if both `damage_taken == 0`, else
`#GOODIDEA2` (`0x5082BC`–`0x5082DB`).

That `HP × strength` ratio matches 1-damage conventional rounds and is wrong
when firepower is the receiver's reactor: fusion (20 HP, takes 2) looks twice
as tough as fission (10 HP, takes 1). Thinker's `combat_odds_fix`
(`refs/thinker/src/veh_combat.cpp`) rewrites conventional +
`ignore_reactor_power` as strength × hits-to-kill
(`ceil(HP / own reactor)`). It skips psi (natives are fission; mixed
fusion-vs-worm still shows the HP lie). This repo applies the same
hits-to-kill correction whenever a lost round costs `psi_damage`.

**When the dialog appears** (`0x507CA4`–`0x507D42`): `VehAttackFlags & 1`, not
an intercept, local human, `combat_type == 0`, not a missile (`Chassis.missile`
at `0x94a37d`), attacker state not `VSTATE_EXPLORE | VSTATE_ON_ALERT`
(`test ch, 0x42` at `0x507D21`). `#BADIDEA` always if
`2 * offense * def_fp * atk_hp < defense * atk_fp * def_hp` (`shl edx, 1`
then `jge` skip at `0x507D3E`). `#GOODIDEA` only if
`GameMorePreferences & 0x8000`.

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
`config/deviations/tech_cost_thinker.lua` (not on `GameDataPaths`).

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

## Probe: Incite Drone Riots

Action id 4 in `probe` (`0x59F120`). Success at `0x5A38C5`–`0x5A3A36` does **not**
force a riot for N turns. It lengthens the same `assimilation_turns_left` counter
conquest uses, recomputes psych, and sets the riot flag only if that recompute
already has more drones than talents.

**Assimilation bump** (`0x5A38DD`–`0x5A3912`):
`cap = max(current, 10 * pop_size / 4)` (signed `/4`), then
`assimilation_turns_left = range(current + 10, 0, cap)` via `range` (`0x422F00`).
Typical +10, hard-capped at `2.5 × size`. Already above the cap: no change.
Does not write `faction_id_former`. Then `set_base` (`0x4E39D0`) and
`base_compute(1)` (`0x4EC3B0`).

**Riot flag** (`0x5A3939`–`0x5A395D`): `CurrentBase` `drone_total` (`+0x11C`) vs
`talent_total` (`+0x118`). `jg` (drones > talents) ORs `state_flags` bit 1 and
`draw_tile`. `jle` leaves the flag alone. Script: `DRONEINCITE0` / "Drone riots
incited at $BASENAME0!" vs `DRONEINCITE1` / "Drone activity increased at
$BASENAME0!" (`NetMsg_pop_2` `0x50B9E0`). Same text for actor and target;
shown only if `MapWin->cOwner` is the probe faction or the base owner
(`0x5A398E`–`0x5A3A2E`, `CurrentPlayer` `0x939284`). Art is `drone_sm.pcx`
or `ALdrone_sm.pcx` (alien bit `0x80` on the probe faction). Does **not**
call `drone_riot` (`0x4F4FB0`), so no facility destruction or revolt on
the probe turn. After the popup the common success path (`0x5A36EE`) does
not add another message.

Thinker's `probe` case `PRB_INCITE_DRONE_RIOTS` matches this path (no `Fix:`).

## Probe: action ids, HSA, odds, and the roll (`probe` `0x59F120`)

Action ids in `probe` match Thinker's `ProbeAction`: infiltrate 0, steal tech 1,
sabotage 2, drain 3, incite 4, assassinate 5, mind-control city 6, genetic plague
7, free captured leader 8, mind-control unit −1. Total Thought Control is not a
separate id: it is mind-control city with `prb_diff = 1` from the THOUGHTMENU
choice.

**Hunter-Seeker** (`0x59F3A4`–`0x59F6EA`): `has_abil` of `ABL_ALGO_ENHANCEMENT`
(`0x10000000`) at `0x5BF1F0`. Without it, an HSA owner (`0x59F5D7`) that is
actually executing (`VehAttackFlags` bit 2 and `toggle`) pops `HUNTERSEEKER` /
`HUNTERSEEKER2` and `veh_kill` (`0x5C0B00` at `0x59F6EA`). With Algo,
`ProbeHasAlgoEnhancement` (`0x945B30`) is 1 and the mission may continue with
`ProbeTargetHasHSA` (`0x945B38`) set.

**Displayed odds** (`success_rates` `0x59EEE0`): morale floored at 1. Risk &lt; 0
prints `100%` and returns the negative modifier. Covert Ops Center (facility
`0x22`) adds +2, then `SE_probe + that` is clamped to `[−2, 0]`. Failure
`(risk * 100) / ((morale / 2) − defense + 1)`. Algo and not HSA: signed `/2` of
failure. Success `100 − failure`; HSA signed `/2` of success. Loss
`((risk + 1) * 100) / (morale − defense)`; Algo and not HSA: `/2` of loss
(`0x59F09A`–`0x59F0AB`, same gate as failure); survival
`100 − loss`. HSA then halves **success again** (`edi`) instead of survival
(`esi`) at `0x59F0B4`–`0x59F0BF`. Thinker's `mod_success_rates` applies the
second HSA `/2` to survival (`Fix:`).

**Actual mission roll** (`0x5A311C`–`0x5A3238`): `game_randv(morale/2 − defense + 1)`
compared to risk (`jge` at `0x5A31A0` = success). On failure: busted unless Algo
and not HSA and `random() & 1` is clear (`0x5A31A6`–`0x5A31BF`). On success: HSA
still busts when `random() & 1` (`0x5A3228`–`0x5A3238`). Risk 0 therefore always
passes the strength check; only HSA's coin can still eliminate the probe.
`BUSTED` is `0x5A31F0`. Thinker's `probe` rewrite matches this two-step roll.

**Frame** (`FRAME` `0x690668` at `0x5A2F1B`, `FRAMEMENU` `0x690670`): after a
hostile mission against an AI, a human actor may pick another commlink faction
as the implicated party (stored in `Veh.probe_action` bits 5+). An AI actor
with morale ≥ 5 may implicate the human `RankingFactionIDUnk1` when that
target has a commlink and is not at Vendetta. Busting while a frame is set
negates the id (`0x5A31D1`).

- Frame flop (`prb_state < 0`, `0x5A4BA0`): `FactionStatus` `0x9A64E8` bit
  of the framed id is the human test. AI framed: `FRAMEFLOP`;
  `treaty_on(framed, actor, VENDETTA)` at `0x5A4C2F` always. Human framed:
  no automatic Vendetta. MP (`0x93F660`): `HAVEFRAMEEXCUSE` /
  `HASFRAMEEXCUSE` only (`agenda_on` `0x20` at `0x5A4D4F`). SP Pact:
  `PACTFRAMEEXCUSE` (`popp_2` `0x50B970`); a yes calls `pact_ends` at
  `0x5A4E55`. SP otherwise: `FRAMEEXCUSE`; a yes calls
  `treaty_on(actor, framed, VENDETTA)` at `0x5A4EB8`. Then the target's
  excuse dialogs still name the actor.
- Human target (`is_human` at the `EXCUSE` / `PACTEXCUSE` / `NETEXCUSE`
  block `0x5A4EE7`): the incident is with the **actor**, not the framed
  faction. The successful-frame path is not reached. MP mind-control-city or
  worse (`action_id >= 6`) `inc` the actor's `integrity_blemishes`
  (`0x5A4FC4`).
- Successful frame (`prb_state > 0`, `0x5A53B3`): `treaty_on(target, framed,
  COMMLINK|VENDETTA|0x40)` at `0x5A53C2`, `set_treaty` `DIPLO_WANT_REVENGE`;
  `FRAMEJOB` / `FRAMED`. No `cause_friction` and no blemish write.

The two `cause_friction` sites (`0x5A55D0`, `0x5A5791`) run only after that
frame-success return, and only when the pair still has Truce/Treaty/Pact
(`has_treaty` mask `0x7` at `0x5A4B2B`; Vendetta skips the whole aftermath).
Args are `cause_friction(target, actor, game_randv(4))` — delta `0..3`,
victim resents the **actor**. Thinker's `mod_probe` matches the frame
retarget; its Total Thought Control `cause_friction(..., 5)` is a `Fix:`.

**Agreement aftermath** (`0x5A4B15`–`0x5A5813`). `ebx` actor, `esi` target,
`[ebp-0x18]` action id, `[ebp-0x30]` the mission risk (Thinker's `prb_diff`, the
value the roll at `0x5A3198` compares against; Total Thought Control is
mind-control city with it nonzero).

- Entry `0x5A4B15`: action 0 (infiltrate) returns. Busted missions jump here
  too (`0x5A3223`, `0x5A3298`, after `veh_kill`), so failures get the aftermath.
- Unit subversion (−1) jumps from `SUBVERTED` straight to `0x5A4B28` when its
  risk is 0 (`0x5A42B8`) and so is an action below 6 below.
- `act_of_aggression(actor, target)` at `0x5A4AA4` is mind-control city only:
  `0x5A4783` sends every other action to `0x5A4ABD`, past it, and TTC's nonzero
  risk skips it at `0x5A4A9D`. Mind-control city therefore reaches `0x5A4B2B` at
  Vendetta and returns there; every other action arrives with its agreement.
- `0x5A4B47`: target→actor Pact with `DIPLO_HAVE_SURRENDERED` (`0x2000000`)
  returns. `0x5A4B76`: TTC sets `0x420` (`SHALL_BETRAY|WANT_REVENGE`) and returns.
  Frame and human-target branches follow (above).
- `0x5A5515`: `diplomacy_check(actor, target, 0)` (`0x53A980`) refills the
  `Diplo*` globals for this pair, then `diplomacy_caption`.
- **Pact** (`has_treaty(actor, target, PACT)` at `0x5A556F`):
  - Steal tech (1), target `tech_ranking` > actor's (`jle` `0x5A55AD`), actor
    `ranking` < 6 (`0x5A55AF`), `!DiploGreatSatan`: friction at `0x5A55D0`,
    `KEPTPACT`. Pact kept.
  - Else action < 6, actor `ranking` < target's and actor `ranking` < 5
    (`0x5A5636`–`0x5A5641`): `EXCUSEDPACT` only. Pact kept, no friction.
  - Else `USEDEXCUSE`. Action < 6: `pact_ends(actor, target)` (`0x53C650`, at
    `0x5A5691`) — `treaty_off` PACT plus `pact_withdraw` both ways; `treaty_on`
    PACT also sets TREATY, so the pair is left at Treaty. No `double_cross`, no
    blemish. Action ≥ 6: `double_cross(actor, target, -1)` at `0x5A56A1`, then
    `treaty_on` VENDETTA.
- **Truce / Treaty** (`0x5A56B7`):
  - target→actor `DIPLO_WANT_REVENGE` (`0x20`) or action ≥ 6: break.
  - Steal tech (1) or assassinate (5) with the Pact steal-tech gates and
    `game_randv(8) != 0` (`0x5A571B`): keep.
  - Sabotage (2) or incite (4): keep when `DiploFearScale && !DiploGreatSatan`;
    else when target `ranking` > actor's, `!DiploTributeDemand`, and
    `game_randv(8) != 0` (`0x5A5779`).
  - Drain (3) and every failed gate: break.
  - Keep: friction at `0x5A5791`, `KEPT%d` by action id. Status unchanged.
  - Break (`0x5A57D6`): `USEDEXCUSE`. Action < 6: `treaty_on(actor, target,
    VENDETTA)` at `0x5A5801` — no `double_cross`, so no blemish and no
    `intervention`. Action ≥ 6: `double_cross(actor, target, -1)` at `0x5A580B`.
- Thinker's `mod_probe` matches every branch above.

Inputs on those paths: `Faction.ranking` (`+0x4`, `0x96C9E4`) is the 0 (lowest)
to 7 (highest) power rank `rankings` assigns by score (Thinker replaces
`0x5AC690`). `Faction.tech_ranking` is `+0x35C` (`0x96CD3C`, see TECHS).
`DiploGreatSatan` (`0x93FA3C`, written `0x53AA73`–`0x53AAE6`) is `great_satan`
for the pair. `DiploFearScale` (`0x93FA98`, `0x53B10D`–`0x53B4EB`) is the
target's fear of the actor's force. `DiploTributeDemand` (`0x93FA88`,
`0x53B2F5`–`0x53B462`) is the energy the target would demand. All three are
`diplomacy_check` outputs.

**Escape roll** (base targets, `0x5A42CA`–`0x5A4466`): `game_randv(morale − defense)`;
`jle` risk at `0x5A4306` = lost. A lost probe is saved (returns home) only when Algo
and not HSA and `random() & 1` is set (`0x5A4454`–`0x5A446D`). On escape, HSA's
`random() & 1` coin (`0x5A4427`–`0x5A443B`) is reached only for a sea-triad probe
whose return base is not a port; every other escape skips it. Thinker matches.

**Infiltrate bit** is set on commit, before the roll: `set_treaty` (`0x55BB30`)
with `DIPLO_HAVE_INFILTRATOR` (`0x1000`) at `0x59FBA0`. `DETECTINFILTRATE` follows
at `0x59FBC1`. A busted infiltrate still leaves the bit.

**Steal tech** success ORs `BSTATE_RESEARCH_DATA_STOLEN` (`0x40`) at `0x5A33C2`.
A later attempt at that base tests the same bit (`0x59FE2B`) and uses risk 1.

**Drain energy**: `steal_energy` (`0x50C4B0`) is `energy_credits * pop_size /
(pop_total + 1)` when energy &gt; 0. The success path (`0x5A371B`–`0x5A3783`)
then `floor = share * morale / 6` (magic `0x2AAAAAAB` at `0x5A3739`), adds
`game_randv(share − floor) / 2`, clamps to the treasury (`range` `0x422F00`),
and stores 0 if the result is 1. Thinker's `steal_energy` multiplies by
`conf.steal_energy_rate` (not original). `probe` **tests**
`BSTATE_ENERGY_RESERVES_DRAINED` (`0x400000` at `0x5A033A`) for a risk-1 repeat
but never ORs that bit — the drain-repeat bump looks dead.

**Mind-control cost** (`mind_control` `0x59EA80`, `vulnerable` `0x59E980`): no HQ
→ distance 12; HQ tile → −1 (refused) unless corner-market. Then Genejack ×2
dist, Creche /2, Punishment Sphere /2, nerve staple /2. Cost
`(combat+offense garrison × (poly-encryption + 1) + actor mind_control_total/4 + pop)
* (corner_market_cost + energy + 1200) / (dist + 4)`, then pact ×2, former owner
/2, drone riot /2, golden age ×2, atrocity victim ×2 else revenge ×1.5. Probe()
then applies `RULE_PROBECOST`, Nethack ×3/4, and the SE Probe table. SE ≥ 3 is
immune even with Algo in this binary; Thinker's Fix 1 & 15 lets Algo through at
×2.

The garrison term is three `stack_check` (`0x5B9580`) calls on the stack at the base
tile (`0x59EC57`–`0x59ECB8`): mode 2 plan `PLAN_COMBAT` (1) plus mode 2 plan
`PLAN_OFFENSE` (0), times (mode 6 count of units with `ABL_POLY_ENCRYPTION` + 1).
The faction argument is −1, so every unit in the stack counts, an allied unit
included. `PLAN_DEFENSE` and every non-combat plan count 0. Poly is a stack
multiplier, not a per-unit weight: three offense units with one Poly unit among
them count 6, and a Poly defender still raises the multiplier. Thinker's
`mind_control` matches.

Distance is `vector_dist` (`0x4F8090`, inlined in `vulnerable` at `0x59EA0A`–`0x59EA6B`):
`max(dx, dy) − ((dx + dy)/2 − min(dx, dy) + 1)/2` in map coordinates, x wrapping on
round maps. On every same-parity offset it equals `TabletopDiagonalDistance` on the
lattice. The HQ-tile refusal tests that raw distance, before the facility halvings; a
halved distance of 0 is quoted at `/ (0 + 4)`. The halvings are sequential integer
`/2`, after Genejack's `×2`. After the formula, an AI actor (`FactionStatus`
`0x9A64E8` bit clear) against a human target whose `diff_level` > 3 pays
`cost * 3 / diff_level` (`0x59ED0C`–`0x59ED3C`): ×3/4 on Thinker, ×3/5 on Transcend.
Thinker's `mod_mind_control` matches.

**Unit subversion cost** (`0x5A14BF`–`0x5A15A2`): HQ distance as above (`vector_dist`
at `0x5A16EA`, 12 with no HQ); no HQ-tile refusal. A target whose home base
(`VEH` +0x2E, `≥ 0`) has the Punishment Sphere (facility 23 via `bitmask` at
`0x5A14E0`) halves the distance; no Crèche, Genejack or nerve-staple term. Cost
`unit_cost * ((energy + 800) / (dist + 2))`, dividing first (`idiv` at `0x5A1546`,
`imul` by `UNIT` +0x29 at `0x5A1565`). Then the same AI-vs-human `× 3 / diff_level`
(`0x5A1552`–`0x5A15A2`), `RULE_PROBECOST`, Nethack ×3/4, the SE Probe table, Poly
and plan below. The unit path reads no diplomatic status (`Faction` `0x96C9F8`) and no
atrocity record, so Pact, former owner, riot, golden age and atrocity modifiers are base-only.
Thinker's `probe` matches.

**Unit subversion and Poly Encryption**: after the SE Probe table, a target unit
with `ABL_POLY_ENCRYPTION` (`0x1000`) doubles the subversion cost (`has_abil` at
`0x5A17C6`, `add esi,esi` at `0x5A17D2`). Then a target whose `UNIT.plan`
(`0x9AB892`) is neither `PLAN_COLONY` (8) nor `PLAN_TERRAFORM` (9) halves it
(`0x5A17D7`–`0x5A17FC`): colony pods and formers cost full price, everything else
half. Thinker `probe.cpp` matches.

### Unit plan (`make_proto` `0x5A5D40`)

`UNIT.plan` (`+0x2A`, `0x9AB892`) is the design's AI role, set when a design is made.
Predefined units take it from `alpha.txt` instead. Thinker's `mod_make_proto` matches:

- Missile chassis (`0x94A37D`): offense ≥ 99 → `PLAN_PLANET_BUSTER` (5); Tectonic
  Payload (`0x18`) → 13; Fungal Payload (`0x19`) → 14; else `PLAN_OFFENSE` (0)
  (`0x5A5F63`–`0x5A5FC1`).
- Weapon mode ≥ 7 (non-combat): plan = mode, so transport 7, colony 8, terraform 9,
  supply 10, probe 11, artifact 12 (`0x5A5FC6`–`0x5A5FDE`).
- Sea triad: `PLAN_NAVAL_SUPERIORITY` (6) (`0x5A5FF7`).
- Air triad: `PLAN_AIR_SUPERIORITY` (4) with ability `0x20`, else `PLAN_OFFENSE`
  (`0x5A6012`–`0x5A6045`).
- Land (offense and defense are the raw `Weapon.offense_value` `0x94AE68` and
  `Armor.defense_value` `0x94F280` bytes, compared unscaled at `0x5A6081`–`0x5A608D`):
  `PLAN_OFFENSE`; armor defense > 1 → `PLAN_DEFENSE`; then weapon offense ≥
  armor defense while `PLAN_DEFENSE` → `PLAN_COMBAT`; speed > 1 and offense > defense
  → `PLAN_OFFENSE`; Amphibious, Drop Pods or Artillery (`0x8018`) → `PLAN_OFFENSE`
  (`0x5A604A`–`0x5A60E9`). Offense 1 and defense 1: speed > 1 → `PLAN_RECON` (3);
  Police 2× (`0x400000`) → `PLAN_DEFENSE` (`0x5A618D`–`0x5A61EF`).

## Probe: success / fail popups (`probe` `0x59F120`)

`NetMsg_pop_2` (`0x50B9E0`) unless noted. Shown when `MapWin->cOwner` (`0x939284`)
is the probe faction (actor) and/or the target, as listed. Suffix `0` is actor,
`1` is target (`says` `0x500130` after `clear` `0x500120`). Art is
`infil_sm.pcx` / `al_infil_sm.pcx` unless noted.

| Script | Who | When |
|---|---|---|
| `DETECTINFILTRATE` (`0x59FBC1`–`0x59FC78`, `NetMsg_pop` `0x47A890`) | target | Infiltrate set the infiltrator bit |
| `DECIPHERED0` / `DECIPHERED1` (`0x5A3361`) | actor / target | Steal tech |
| `STOLENOTHING` (`0x5A029A`, `NetMsg_pop`) | actor | Pre-mission abort when nothing to steal and the map is already known |
| `GOTENERGY` / `GOTNOENERGY` (`0x5A3826`) | actor | Drain; `GOTNOENERGY` when the stolen amount is 0 |
| `TOOKENERGY` / `TOOKNOENERGY` (`0x5A38AA`) | target | Drain |
| `PRODVIRUS0` / `PRODVIRUS1` (`0x5A3425`) | actor / target | Sabotage wiped production |
| `FACVIRUS0` / `FACVIRUS1` (`0x5A363D`) | actor / target | Sabotage destroyed a facility. Thinker's `mod_probe` inverts this suffix (`==` vs `!=`) |
| `DRONEINCITE0` / `DRONEINCITE1` | both | Incite; see above |
| `ASSASSINATED` (`0x5A3ACF`) | both | Assassinate; `$NUM0` is the points actually subtracted |
| `MINDCONTROL0` / `MINDCONTROL1` / `MINDCONTROL2` (`0x5A48B6`) | actor / target / observer | Mind-control base (`popp_2` `0x50B970`) |
| `GENETICWARFARE0` / `GENETICWARFARE1` (`0x5A40B4`) | actor / target | Plague (`popp_2`) |
| `SUBVERTED` (`0x5A42A6`) | target | Mind-control unit |
| `BUSTED` (`0x5A31F0`) | actor | Mission failed (probe eliminated). `BUSTED1`–`7` are excuse-dialog labels, not this popup |
| `PROBECAUGHT` / `PROBESUCCUMB` (`0x5A4AE6`) | actor | Mission succeeded, probe died; `PROBESUCCUMB` when the action was mind-control city |

Infiltrate has no actor success script (the actor gets `PROBESURVIVAL` on the
return-home path). Unit subvert has no actor script.

### Extra drones (`base_psych` `0x4EA540`)

At `0x4EA62E`–`0x4EA6AF`, capture drones are
`max(0, min((pop + (human ? Factions.diff_level : 3) − 2) / 4, (assim + 9) / 10))`
and add into the drone accumulator. Human vs AI is `FactionStatus` (`0x9A64E8`).
+10 assim turns is one extra drone (`19 / 10`), then the size/difficulty cap.

### Counter decay (`base_upkeep` `0x4F7F58`–`0x4F7F8E`)

If `assimilation_turns_left != 0`: decrement. If `faction_id_former == faction_id`
and still nonzero: decrement again. Hitting 0 copies current owner into former.
`base_init` (`0x4E4C65`–`0x4E4C78`) founds with former = current and assim = 0, so
an incite on a never-captured (or fully assimilated) base lasts **5** upkeeps.
A still-assimilating captured base (former ≠ current) decays one per upkeep.

Thinker's `mod_base_upkeep` decrement matches. `mod_base_init` also sets former =
current; its `memset` of the whole `BASE` is marked `Fix:`.

### Flag lifetime (`base_drones` `0x4F5CE0`)

At `0x4F5DCC`: if drones > talents, no nerve staple (`+0x8`), and no Telepathic
Matrix (`SecretProjects` `0x9A658C` owned by this faction), `jmp drone_riot`.
Otherwise, if bit 1 is set, clear it (`and 0xFD` at `0x4F5E18`) and popup
`DRONERIOTSOVER`. The probe-set flag is the same bit: it ends when drones no
longer exceed talents (or staple / Matrix), not after a fixed turn count.

The flag still costs the target one turn of minerals. `base_upkeep` calls
`base_production` (`0x4F7A2F`) before `base_drones` (`0x4F7B3D`), and
`base_production` adds `mineral_surplus` (`+0xD4`) to `minerals_accumulated`
(`+0x40`) only when bit 1 is clear (`0x4F08EC`–`0x4F0907`; also skipped for a
negative surplus or a Stockpile Energy queue head, `-0x45`). Thinker's
`mod_base_production` keeps the same test.

## Diplomacy standing and talk

Thinker names from `engine.cpp` / `engine_types.h`. Confirmed in this binary.
Thinker's diplomacy patches (loans, tech-for-energy, base-swap) are in
`Details.md`; the functions below are vanilla.

`Factions` `0x96C9E0`, stride `0x20CC`.

| Offset | Field | Meaning |
|--------|--------|---------|
| `+0x18` (`0x96C9F8`) | `diplo_status[8]` | bits: Pact `0x1`, Treaty `0x2`, Truce `0x4`, commlink `0x8`, Vendetta `0x10`, infiltrator `0x1000`, atrocity victim `0x40000`, major-atrocity victim `0x80000`, surrendered `0x2000000` |
| `+0x58` (`0x96CA38`) | `diplo_friction[8]` | directed 0–20; our "grievances" |
| `+0x124` (`0x96CB04`) | `integrity_blemishes` | per-faction count, 0 = Noble; editor caption "Blemishes" |
| `+0x128` (`0x96CB08`) | `diplo_reputation` | incremented on made Pact/Treaty/Truce |
| `+0x12C` | `diplo_gifts[8]` | subtracted from blemishes in `reputation()` |

### `cause_friction` (`0x53A030`)

`void(faction, other, delta)`. `friction[faction][other] = clamp(old + delta, 0, 20)`.
If the cached talk pair (`DiploFrictionFactionID` `0x93FAC0` /
`DiploFrictionFactionIDWith` `0x93FABC`) matches, also add `delta` to
`DiploFriction` `0x93FA74`. 19 call sites.

### `get_mood` (`0x53A090`)

`int(friction)` → 0..8. Bands: ≤0 Magnanimous, 1–2 Solicitous, 3–4 Cooperative,
5–8 Noncommittal, 9–12 Ambivalent, 13–15 Obstinate, 16–17 Quarrelsome,
18–19 Belligerent, ≥20 Seething. Labels from `alphax.txt` `#MOOD` (9 names),
pointer table `0x94C9E4` (filled by `read_rules` `0x5873C0` from the `#MOOD`
section). Three call sites, all `return` as an index into that table for a
draw string: `DiploPop_draw_portrait` `0x43FA34`, `InfoWin` text `0x456DFC`,
`ReportWin_draw_intel` `0x4A2750`. No AI or talk function calls it.
`#REPUTE` (`0x946A30`, eight names) is the same pattern: portrait `0x43F971`
and InfoWin `0x457067` only.

### `reputation` (`0x53A100`)

`int(faction, other)` = `clamp(integrity_blemishes - diplo_gifts[other], 0, 99)`.
AI uses this; the portrait does not.

### `diplomacy_caption` (`0x5399A0`)

Same mood bands on cached `DiploFriction`. If `DIPLO_VENDETTA` and friction < 15,
floors the caption to friction 15 (Obstinate) without writing the store.

### Integrity display (`DiploPop` `0x43F955`)

`clamp(integrity_blemishes, 0, 7)` indexes `#REPUTE` (8 names: Noble, Faithful,
Scrupulous, Dependable, Ruthless, Treacherous, Wicked, Infamous). Counts above
7 still draw Infamous.

### Integrity / friction writers

- `double_cross` `0x50BCC0`: `inc` blemishes; `inc` wrongs/betrayed; friction −4/−2
  on some paths; Vendetta via `treaty_on` `0x10`; `pact_ends`. Calls
  `intervention(def, atk)` at `0x50C2C8`.
- `intervention` `0x50BA50` (`faction_id_def`, `faction_id_atk`): each living Pact
  partner of `def` that is not already at Vendetta or Pact with `atk` joins via
  `treaty_on` (`0x55B870` at `0x50BC45`) `DIPLO_VENDETTA|commlink|0x80000000`. No
  `double_cross`, so pact honor writes no agreement-break blemish.
- `act_of_aggression` `0x50C2E0`: `double_cross(atk, def, -1)` when the pair holds
  Truce/Treaty/Pact, then `treaty_on` Vendetta. Combat hostility; not called from
  `atrocity` / `major_atrocity` / `planet_busting`. `probe` calls it only for
  mind-control city (`0x5A4AA4`).
- `atrocity` `0x55EEE0`: ORs `0x40020` (`DIPLO_ATROCITY_VICTIM|DIPLO_WANT_REVENGE`)
  onto **tgt→perp** at `0x55EF0D` and saves the old `0x40000` bit for first/repeat,
  before `un_charter` (`jg` `0x55EF3E`). Charter on: friction +5; `inc` blemishes
  unless that saved bit. Does not call `double_cross`. Calls `major_atrocity` at
  `0x55F29F` on some paths.
- `major_atrocity` `0x55F450` (`faction_id`, `faction_id_tgt`). `tgt >= 0` ORs
  `0xC0020` (`DIPLO_MAJOR_ATROCITY_VICTIM|DIPLO_ATROCITY_VICTIM|DIPLO_WANT_REVENGE`)
  onto **tgt→perp** `diplo_status` and agenda bit 2, even if the Charter is
  repealed. Penalties (`jg` at `0x55F4A6`) run only while
  `ProposalPassCount[8] <= ProposalPassCount[9]` (`un_charter`). Then:
  `tgt < 0` → `integrity_blemishes += 2` (no floor, no cap) at `0x55F51E`;
  `tgt >= 0` and perp→tgt lacks `0x80000` → `clamp(blemishes+2, 6, 99)`;
  perp→tgt already has `0x80000` → skip the blemish write (`jne` `0x55F4CE`).
  The first/repeat test is the opposite direction from the OR, so a second
  strike on the same victim still takes the floor path unless something else
  set perp→tgt. Charter on: always `major_atrocities++` and living AI (not
  human, not perp, not tgt) join Vendetta. No `cause_friction`. Does not call
  `double_cross`. Thinker's rewrite matches these branches.
- `probe` `0x59F120`: agreement aftermath only (see Agreement aftermath under
  Probe). `inc` actor blemishes at `0x5A4FC4` (MP, human target, `action_id >= 6`).
  `cause_friction(target, actor, game_randv(4))` at `0x5A55D0` and
  `0x5A5791` (delta `0..3`) when the agreement is kept. A broken agreement
  blemishes only through `double_cross` (`action_id >= 6`). Successful frame
  writes Vendetta on the implicated faction and skips both.
- `battle_fight_2` `0x506AF0`: friction +3 (`0x5078BB`), −1 (`0x50A1AB`), +5
  (`0x50B1EF`).
- `action_terraform` `0x4C9B00`: friction +5 at `0x4CA3EB`.
- `communicate` `0x54FFD0`: friction −1/−2/+1 and computed gift deltas; can
  `dec` blemishes when count > 1 (`0x557A99`).
- `setup_player` `0x5B0E00`: zeros blemishes and `diplo_reputation` (`0x5B1210`).

No per-turn friction decay among the 19 `cause_friction` calls.

### `planet_busting` (`0x500B60`)

`void(veh_id, tx, ty)`. Blast radius `TableRange[reactor]` when `UNIT.plan`
is `PLAN_PLANET_BUSTER` (5), else 0. A pre-pass (`shoot` `0x500560`) can abort
the detonation (missile still dies); territory (`whose_territory` `0x4E3EF0`)
is only that abort check, not the atrocity victim.

On a real blast, one scan of `TableOffset*` tiles, impact first:

1. `base_at` (`0x4E3A50`): first **foreign** base owner (byte `BASE+0x4` at
   `0x97D044` ≠ detonator) becomes the victim; own bases are killed
   (`base_kill` `0x4E5250`) and skipped.
2. Else units on the tile (`veh_at` / `stack_fix`): first **foreign** unit
   owner (`VEH+0x1A` at `0x952836`).
3. If both stayed `-1` (empty disk, or only the detonator's bases/units),
   the victim is `-1`.

Then `major_atrocity(detonator, victim)` at `0x5010AC`. A victimless buster
is still a major atrocity: Charter on, `+2` blemishes and `major_atrocities++`
and the AI rally, with no victim flags. `shoot` and `major_atrocity` do not
call `double_cross`, so a buster from Treaty takes no agreement-break blemish.
Thinker's `planet_busting` matches this victim order; its `Fix:` is only
placeholder draw veh ids.

### Talk

Scene labels in `xscript.txt`, not a chain state machine: `INTRONEW0–5`,
`INTRO0–3`, `INTROMAD0–4`, `DIPLOMENU`, `PROPOSALMENU`, `VENDETTA0–18`.
Per-faction `FACTIONTRUCE` / `FACTIONTREATY` in each faction file.
`introduce` `0x53D360`, `diplomacy_menu` `0x54C560`, `proposal_menu` `0x54DCF0`,
`communicate` `0x54FFD0`. `introduce` picks `INTRO` / `MAD` / `NEW` from
`diplo_status` bits (`0x8000000`, `0x20`), not from `get_mood`. `get_patience`
`0x53A150` uses raw cached `DiploFriction` and status, not the mood index.

### AI (not fully mapped)

`propose_pact` `0x543BC0`, `propose_treaty` `0x544E40`, `wants_to_attack`
`0x55BC80`, `enemy_diplomacy` `0x55F930`, `great_satan` `0x539C00`. They read
friction, `reputation()`, `DiploGoodwill` `0x93FA9C`, status, ranking.
