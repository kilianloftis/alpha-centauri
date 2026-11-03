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

### Disengage (`battle_fight_2` `0x506AF0`)

Only the defender withdraws. `retreat_value` (`[ebp-0x24]`) is zeroed at `0x508C9F` and set
only when `veh_speed(def) > veh_speed(atk)` (`0x508CB8`) and the remaining Thinker conditions
hold (no artillery, defender armed, neither side air, Comm Jammer vs Dissociative Wave, not
on Hold, no base, airbase or bunker, alone on its tile). Its value is
`(HP + 1) / 2` (`0x508E27`–`0x508E2D`).

Each round the defender loses while alive, `retreat_value −= ` damage (`0x5093DE`–`0x5093EB`).
At `≤ 0` it is zeroed (`0x509400`) and the adjacent tiles are searched for a retreat. There is
no `rand()` in the block (`0x5093F1`–`0x509740`), so the withdrawal is one deterministic try,
and with no tile found it is never retried.

### Artillery strike vs duel (`battle_fight_2` `0x506AF0`)

`combat_type` lives in `[ebp+0x18]` (the render path writes it 0 / 1 at `0x50880A` /
`0x5088DA`). A plain strike (`combat_type == 1`, `cmp` / `jne 0x508B50` at `0x5084AB`) takes its
own branch, `0x5084B4`–`0x508B4F`. For each unit on the target tile it calls `veh_skip` on the
attacker (`0x5085CF`) and `battle_compute(..., 1)` (`0x5085E0`). It then applies one damage roll,
capped by `max_dmg_percent_arty_*` (the formula follows Thinker and is not traced here). Units
left at 0 HP are removed with `stack_kill` (`0x508AAE`), and the branch returns 0 at `0x508B4F`.
No jump leaves the branch, so a strike never reaches `promote` (`0x5062B0`, called only at
`0x50A2D5` / `0x50A42A`) or `battle_kill` (`0x505D80`, called only at `0x50A8F4` / `0x50ABB0`).
That means no promotion for either side, no artifact capture and no collateral damage.

A duel (`combat_type == 3`, `CT_CAN_ARTY | CT_WEAPON_ONLY`) skips that branch and runs the
normal round loop. It then makes the same `promote` calls as any fight, at the same rate.

### Defender selection (`best_defender` `0x5044D0`)

Walks the defender's stack (`next_veh_id_stack`, `VEH+0x952858`, loop at `0x504A7F`) and keeps the
highest score. For each unit it calls `battle_compute` (`0x50474C`) and stops the whole scan when
`offense_out` is 0 (`je 0x504A91` at `0x504759`). Then:

- HP: 1 for an artifact (`UNIT.plan == 0xC`, `cmp bl, 0xc` at `0x504775`), else
  `clamp(reactor_id, 1, 100) * 10 − damage_taken`, clamped `[0, 9999]`; max HP the same without
  the damage (`0x504775`–`0x5047F0`). `value = defense_out * HP / maxHP` (`0x5047F2`–`0x5047F7`).
- `score = attacker_offense * value / offense_out / 8 − Weapon.offense_value` of the candidate
  (`0x94AE68`, `0x504801`–`0x504821`), so of two equal defenders the one with the weaker gun
  defends.
- `score <<= 4` (×16) when `plan < 8` or `plan == 9` (`cmp bl, 8` / `jb`, `cmp bl, 9` / `jne`,
  `shl esi, 4` at `0x504823`–`0x50482D`): every combat plan, naval transport and terraformers.
  Colony (8), supply (10), probe (11), artifact (12) and the special missiles keep their score.
- `+0x80000` (`0x504A61`) for: an air candidate against an air attacker when either side has Air
  Superiority (ability `0x20`, `0x50485D`–`0x5049D6`); in an artillery exchange (`combat_type`,
  `[ebp+0x10]`) a candidate that `can_arty` (`0x5C0DB0` at `0x5049EF`); otherwise a designated
  defender (`VEH.state & 0x80`, `0x504A58`) on a base, an airbase, a carrier stack, or a stack
  with no air units (`stack_check` `0x5B9580` at `0x504A09` / `0x504A4C`).
- Ties: the packed key is `(score << 11) + veh_id`, replaced only when strictly greater (`jle` at
  `0x504A74`), so the higher `veh_id` wins a tie.

Before scoring (`0x5045A0`–`0x504636`, read with Thinker), a land unit on open ocean is skipped,
and a probe attacker (plan 11, `cmp ..., 0xb` at `0x504602` / `0x50460C`) only considers probes.
Thinker's `find_defender` (`refs/thinker/src/veh_combat.cpp`) keeps all of this and replaces the
packed key with a score / id pair to avoid overflow.

### Last defender lost (`battle_fight_2` `0x506AF0`)

After an attacker win, once the defender and any stack casualties are removed, a base on the
target tile (`[ebp-0x20] >= 0`, `0x50AC90`) gets state bit `0x8` (`0x50ACAA`) and loses one
population (`dec` at `0x50AD8E`–`0x50AD98`; size 0 calls `base_kill` `0x4E5250` at `0x50ADA8`)
unless any of these holds:

- `is_ocean` (`0x5001E0`, `0x50ACB1`) on the attacker's square; Thinker's `Fix:` tests the base
  tile instead.
- The base is an objective (`0x5AC060`) at size 1 or less (`0x50ACC2`–`0x50ACD5`).
- It has facility 4 or 5, Perimeter Defense or Tachyon Field (`0x421670` at `0x50ACE0` /
  `0x50ACF4`), or `0x4F80D0(5, ...)` passes (`0x50AD0A`; Thinker's `Fix:` says the original asks
  the wrong faction).
- The killed defender has no weapon: `Weapon.offense_value` (`0x94AE68`) of its unit is 0
  (`0x50AD1A`–`0x50AD42`, Thinker's `combat_defend`).
- The attacker is native life, faction 0 (`[ebp-0x10]`, `0x50AD48`).
- Any unit is still on the tile (`veh_who` `0x500250` at `0x50AD5B`).
- The defender is human and plays at difficulty 0 (`0x50AD6B`–`0x50AD8C`).

A missile attacker is still on the map at this point. `battle_fight_2` removes it at the very end
(`Chassis.missile` test at `0x50B807`, `kill` `0x5C0B00` at `0x50B812`), so a missile that kills
a base's last defender costs the base population like any other attacker.

## Unit capture in combat

Two ways a fight hands units to the attacker: Marine Detachment boarding, and alien artifacts.
The probe team's Mind Control is separate (see the Probe sections). Thinker's
`mod_battle_fight_2` and `battle_kill` (`refs/thinker/src/veh_combat.cpp`, `veh.cpp`) match
everything below.

### Marine Detachment (`battle_fight_2` `0x506AF0`)

**Eligibility** (`0x508E33`–`0x508F5B`), checked once before the rounds. Every test must pass,
or `detach_value` (`[ebp-0x80]`) stays 0:

- attacker has ability `0x4000000` (`ABL_MARINE_DETACHMENT`, `has_abil` `0x5BF1F0`);
- attacker chassis triad is sea (`Chassis+0x94A379 == 1`);
- the target tile is ocean (altitude bits `& 0xE0 < 0x60`);
- the defender's triad is sea;
- the defender is not native: a predefined unit (`unit_id < 0x40`) with a psi weapon
  (`Weapon.offense_value < 0`) fails, as does Spore Launcher (`unit_id == 0xF`);
- no base on the tile (`base_id` `[ebp-0x20] < 0`).

Then `detach_value = 2 * HP / 3`, where HP is the defender's hit points at the start of the
fight (`clamp(reactor, 1, 100) * 10 − damage_taken`, clamped `[0, 9999]`).

**Roll** (`0x5092EA`–`0x509366`), inside the branch where the defender loses the round,
after its damage is applied and before the disengage check. If `detach_value ≠ 0` and the
defender is still alive: `detach_value −= ` that round's damage; if the result is `≤ 0`,
`rand() % 100 < 50` (`0x50935C`–`0x509363`) captures. A failed roll leaves `detach_value`
negative, so **every** later round the defender loses rolls the 50% again until it dies.

**Capture** (`0x50973A`–`0x509921`):

- every unit on the defender's tile (`veh_top`, then `next_veh_id_stack`), cargo included,
  takes the attacker's `faction_id`. Damage is kept;
- home base: `base_find(tx, ty)` (`0x4E3B80`, the nearest base of any owner); kept if the
  attacker owns it and it has `pop_size ≥ 3`, else `−1`;
- `veh_skip` (`0x5C1D20`) on each, which spends all its moves;
- `bit_set(tx, ty, 0x100000, 1)` (`BIT_SUPPLY_REMOVE`): no random supply pod spawns on the
  tile again;
- `owner_set(tx, ty, attacker)` (`0x591B10`) writes `MAP.val2 & 0xF`, the cached faction of
  the tile's occupant (Thinker `veh_owner()`). Territory (`MAP.owner`, `+0x7`) is not
  touched. Probe Mind Control on a unit makes the same call;
- `#WEMARINEDETACH` to the attacker, `#THEYMARINEDETACH` to the victim;
- `marine_detach` (`[ebp-0x50]`) = 1 and the round loop breaks.

**After the loop** (`0x50A44D`): the attacker's normal promotion roll still runs, then
`marine_detach` jumps to the end of the battle (`0x50B203`). Nothing is killed, there is no
collateral, and the combat win/loss tallies and `diplo_combat_*` counts are not touched.

### Probe Mind Control on a unit (`probe` `0x59F120`)

For comparison with boarding (`0x5A41F3`–`0x5A4239`): the unit takes the actor's faction,
`owner_set` on its tile, then `home_base_id = base_find_2(x, y, actor)` (`0x4E3C60`, the
nearest base the actor owns; no population test). `moves_spent` (`VEH+0x28`) = 0, so it
keeps its full moves; `order = ORDER_NONE`; state bits `0x3004200` clear. Damage is kept.

### Alien artifacts (`battle_kill` `0x505D80`)

Artifacts are `UNIT.plan == 12`. In combat they have 1 HP (`cur_hitpoints` is
`!damage_taken`) and a flat defense of 1 that ignores morale (Thinker `get_basic_defense`
returns 1 before the morale multiply). `battle_compute` (`0x501DA0`) tests only for probe
teams (`plan == 0xB`, `0x502149` / `0x502B87`), so terrain, base and other defense
multipliers still apply to that 1. The first round an artifact loses kills it, and rounds it
wins still damage the attacker.

Collateral damage after a kill skips every occupant other than the defender whose plan is
artifact (12), colony (8), probe (11) or supply (10) (`0x50AA47`–`0x50AA60`). An artifact in a
stack is therefore taken only when the stack has no combatant left.

`battle_kill(veh_id, num_killed, num_relics, credits, veh_id_atk, faction_id_atk)` runs for
every unit a fight destroys: the defender, a stack wiped for losing its last combatant, and
each unit of a defeated attacker's stack. The artifact is **captured instead of killed**
(`0x505DC1`–`0x505F67`) when all of these hold:

- `veh_id_atk ≥ 0`, not the artifact itself, and the attacker's faction is not native
  (`faction_id > 0`). A defeated attacker's stack is passed `veh_id_atk = −1`, so a
  defender never captures;
- the attacker is on the map, and its chassis is not a missile (`Chassis+0x94A37D == 0`);
- the attacker stands on land, **or** its stack has spare cargo space
  (`stack_check(atk, 8)` `> 0`: sea cargo capacity minus land units), **or** the artifact has
  `VFLAG_IS_OBJECTIVE` (`VEH+0x8` bit `0x20`, scenario objective).

Capture: `veh_lift`, `damage_taken = 0`, `faction_id = attacker`, `order = ORDER_NONE`,
the per-faction unit counts move, state bits `0x3004200` clear, `veh_drop` on the
**attacker's** tile, `veh_skip`, `++num_relics`. It returns 0 (not killed). Otherwise the
artifact is killed like any unit.

**Diplomacy** in `battle_fight_2`:

- The human treaty-break prompt (`break_treaty` `0x55F770`, at `0x507117`) is skipped for a
  probe team, or for an artifact defender with no base on the tile (`0x5070E4`–`0x5070F2`).
  Taking a lone artifact outside a base therefore never breaks a treaty or pact.
- Thinker skips the `#SURPRISE` sneak-attack notice for an artifact defender too
  (`!veh_def->is_artifact()`); not confirmed in the disassembly.
- If `num_relics > 0`, the defender is not human, and there is no Vendetta
  (`0x50B1BC`–`0x50B1E9`): `cause_friction(defender, attacker, 5)` and
  `treaty_on(... 0x2000)` (want to talk).
- The attacker's promotion roll is skipped when the defender was an artifact (`cmp cl, 0xC` /
  `je` past `promote` at `0x50A296`).
- Popups: `#OURBOOTY1` / `#OURBOOTY2` (count) to the captor. The victim gets
  `#THEIRBOOTY1` / `2` under Vendetta, otherwise `#THEIRBOOTY3` ("has appropriated").

## Sea bases

### `base_init` (`0x4E4B80`)

A faction's first base, `base_count == 1` after the increment, gets the Headquarters bit
(`0x4E4E3A`–`0x4E4E6A`); no later base does, so a faction that loses its Headquarters must
rebuild it.

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

## Climate

### `world_climate` (`0x5C5A30`)

Rebuilds the whole map's derived climate: `world_shorelines` (`0x5C3F70`), `world_temperature`
(`0x5C4170`), `world_rivers` (`0x5C38B0`), `world_rainfall` (`0x5C4470`), `Path_continents`
(`0x59C790`), `world_analysis` (`0x5C55C0`), then `reset_territory` (`0x523DD0`) unless
`0x9B22EC` is set. Rainfall is never updated locally: every in-game change re-runs the full
scan. Its only other callers are the map generators (`world_monsoon`, `world_dune`).

In-game callers:

- `action_terraform` (`0x4C9B00`): condenser or thermal bore (`0x4CA10C`), aquifer
  (`0x4CA127`), raise land (`0x4CA13F`, after `world_raise_alt` `0x5C2380`), lower land
  (`0x4CA199`, after `world_lower_alt` `0x5C23E0`). Planting or removing forest does not call
  it.
- `action_destroy` (`0x4CAA50`): only when the destroyed items include bore, mirror or
  condenser (`test eax, 0x1C00000` at `0x4CAF7B`, call `0x4CAF80`).
- `action_tectonic` (`0x4CE210`, `0x4CE56B`): after `world_raise_alt` per reactor level.
- `planet_busting` (`0x500B60`, `0x500F0E`): after the crater.
- `base_ecology` (`0x4F67F0`): after `world_volcano` (`0x5C6DB0`, `0x4F78C3`).
- `random_events` (`0x51F2E0`): after `world_crater` (`0x5C5C70`, `0x5211A4`).
- `climactic_changes` (`0x524210`, `0x52426E`) and the same code inlined in `turn_upkeep`
  (`0x5258C0`, `0x525B6E`): each one-level sea level step (`MapSeaLevel` `0x94987C`).
- `goody_box` (`0x57A660`): new river source (`0x57B18E`) and earthquake (`0x57B464`, after
  two `world_raise_alt`).
- Editor and load: `MapWin_editor`, `MapWin_UNK4`, `MapWin_click`, `Console_editor_*`,
  `map_menu`, `clear_dirty`, `world_build`.

### `world_rainfall` (`0x5C4470`)

Thinker's `mapgen.cpp` reimplements it faithfully. In-game the inputs include altitude, ocean,
temperature, rivers, forest at altitude 3–4 (scratch bit `0x40`), the dune, mesa, jungle and
Unity landmarks, and condensers.

- **Forest** (`items` bit `0x200000`, `0x5C4816`) at altitude 3–4 sets scratch bit `0x40` on
  `(x + 1, y − 1…y + 1)` and `(x + 2, y)` (`0x5C4823`–`0x5C48FB`). The middle `x + 1` index
  rounds onto the forest itself on even columns and onto E on odd ones, so the marks are NE, E,
  SE, plus the forest tile when x is even. In the scan, a marked tile at altitude ≤ 4 resets
  `rain_steps` and `dry_steps`, floors `moisture_mod` at 0 and resets moisture to `sloping`
  (`0x5C4BFA`–`0x5C4C27`), after the lee-dry bit. A forest therefore keeps a rain belt alive
  across flats it would otherwise dry out over.
- **Condenser** (`items` bit `0x400000`, tested as byte `+0xA` bit `0x40` at `0x5C450D`):
  marks its 3×3 in scratch byte `MAP+6`, bit `0x1` for the first condenser and `0x4` for a
  second (`0x5C456C`–`0x5C4576`). A third adds nothing.
- After the belt result is clamped to 0..2, each mark adds 1 up to 2 **and sets `rain_flag` to
  1** (`0x5C4DA6`–`0x5C4DC9`). `rain_flag` carries east along the row scan, so a condenser also
  feeds `moisture_mod` on tiles past its 3×3 until the belt decays.
- **Smoothing pass** (`0x5C4EB3`–`0x5C4F9A`): any tile with rainfall 0 that has a land
  neighbour at rainfall 2 is set to 1 (`climate_set` `0x591A80`). This runs on the final values,
  condensers included, so a condenser that makes a tile wet also lifts arid tiles one ring
  further out. The centre tile is not tested for land.
- While `CurrentTurn` (`0x9A64D4`) is non-zero, the first changed tile within 2 of a player
  base sets `0x9B22E0`, which drives `action_terraform`'s TERRAMINE / TERRAYOURS popup.

### Regions (`Path_continents` `0x59C790`)

`MAP+3` is the tile's region: a numbered land or water body. `world_climate` (above),
`world_erosion` and `world_validate` run it; Thinker's map generator calls it unchanged.

- Every region is cleared to 0. Rows 1 to `MapAreaY − 2` are scanned row-major, x ascending over
  the row's parity. An unlabelled tile starts a region of its surface; water is altitude < 3
  (`climate & 0xE0 < 0x60`).
- Land ids count up from 1 and stick at 62; water ids count from 65 and stick at 126
  (`0x59C82E`–`0x59C86D`).
- `Path_continent(x, y, id)` (`0x59C520`) floods the region breadth-first. It steps to the 8
  neighbours (`0x66EF50` / `0x66EF74`: (1,−1), (2,0), (1,1), (0,2), (−1,1), (−2,0), (−1,−1),
  (0,−2)) that are unlabelled and on the same surface, with x wrapping unless flat. A tile on row
  0 or the last row is labelled but not expanded from. The region's size (`Continents[id]`
  `0x9AA730`, stride 28) counts every tile popped, polar ones included.
- **Cap** (`0x59C883`–`0x59C9E4`): when a flood took the stuck id (62 or 126), the code looks
  for the smallest region of that surface strictly smaller than the new one (lowest id on ties).
  When it finds one, that region moves to the overflow id (63 or 127) and the new region takes
  its id. Otherwise the new region moves to overflow.
  - The search starts at id 0 for land and 64 for water. Neither is ever assigned, and only
    `Path_continent` and `Path_merge` (`0x59C340`) write sizes, so that empty slot is always the
    smallest. Hitting a cap therefore relabels every tile not yet reached (both surfaces) to the
    overflow id and moves the new region to 0 or 64. Thinker does not patch it.
- Every tile on rows 0 and `MapAreaY − 1` is then set to 63 (land) or 127 (water), adding to that
  overflow size (`0x59CA12`–`0x59CAEF`).

**`region_total_bases[128]`** (`Faction+0x117C`) counts a faction's bases per region.
`enemy_strategy` (`0x561080`, every faction in `faction_upkeep` each full turn) zeroes
(`0x56110C`) and recounts it (`0x56154C`). `base_init` increments it (`0x4E4C28`) and `base_kill`
decrements it (`0x4E53CD`). A capture or `give_a_base` leaves it stale until the next upkeep.

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
as the implicated party (stored in `Veh.probe_action` bits 5+, `shl 5` at
`0x5A30BE`). An AI actor with morale ≥ 5 may implicate the human
`RankingFactionIDUnk1` (`0x5A2E04`) when that target has a commlink and is not
at Vendetta. Busting while a frame is set negates the id (`0x5A31D1`).

- Offer (`[ebp-0x18]` action id at `0x5A2DCD`): ids `1..5` (`jge 6` / `jg 0` at
  `0x5A2DDA`–`0x5A2DDE`: steal tech, sabotage, drain, incite, assassinate) and
  `8` (free captured leader, `cmp 8` / `jne 0x5A3111` at `0x5A2DE0`). Infiltrate
  `0`, unit subversion `−1`, mind-control city `6` (and TTC) and plague `7` skip
  the menu.
- Candidates (loop at `0x5A2FDB`): not the actor, not the target, actor→them
  `DIPLO_COMMLINK` (`test 0x8` at `0x5A2FFF`), alive (`FactionStatus[1]`
  `0x9A64E9` at `0x5A3009`). No Vendetta filter.
- A nonzero framed id adds 1 to the mission risk (`inc [ebp-0x30]` at
  `0x5A3101`), after any targeted-sabotage minimum.
- The aftermath's `has_treaty(actor, target, 0x7)` return (`0x5A4B2F`) and the
  surrendered-Pact return (`0x5A4B47`) come before the frame branches, so a
  frame against a victim with no Truce/Treaty/Pact writes nothing.

- Frame flop (`prb_state < 0`, `0x5A4BA0`): `FactionStatus` `0x9A64E8` bit
  of the framed id is the human test. AI framed: `FRAMEFLOP`;
  `treaty_on(framed, actor, VENDETTA)` at `0x5A4C2F` always. Human framed:
  no automatic Vendetta. MP (`0x93F660`): `HAVEFRAMEEXCUSE` /
  `HASFRAMEEXCUSE` only (`agenda_on` `0x20` at `0x5A4D4F`). SP Pact:
  `PACTFRAMEEXCUSE` (`popp_2` `0x50B970`); a yes calls `pact_ends` at
  `0x5A4E55`. SP otherwise: `FRAMEEXCUSE`; a yes calls
  `treaty_on(actor, framed, VENDETTA)` at `0x5A4EB8`. Then the target's
  excuse dialogs still name the actor.
- After the AI-framed flop's `treaty_on`, `neg edi` at `0x5A4C37` stores the
  framed id back into `prb_state` still negative. The human-framed test
  (`je 0x5A4EE7` at `0x5A4C5B`) skips the human block, and with an AI target
  `0x5A4EF8` jumps to `0x5A53B3` with `eax` unchanged: `jle 0x5A5515` skips
  the successful-frame write, so keep-or-break between actor and target runs.
  Thinker's `mod_probe` instead sets `prb_state = -prb_state` (positive) after
  that `treaty_on` (`probe.cpp` "FRAMEFLOP"), so its `prb_state > 0` block also
  writes the successful frame's Vendetta and skips keep-or-break. Not marked
  `Fix:`.
- Human target (`is_human` at the `EXCUSE` / `PACTEXCUSE` / `NETEXCUSE`
  block `0x5A4EE7`): the incident is with the **actor**, not the framed
  faction. The successful-frame path is not reached. MP mind-control-city or
  worse (`action_id >= 6`) `inc` the actor's `integrity_blemishes`
  (`0x5A4FC4`).
- Successful frame (`prb_state > 0`, `0x5A53B3`): `treaty_on(target, framed,
  COMMLINK|VENDETTA|0x40)` at `0x5A53C2`, `set_treaty` `DIPLO_WANT_REVENGE`;
  `FRAMEJOB` / `FRAMED`, then `jmp 0x5A5816` (`0x5A5510`). No `cause_friction`
  and no blemish write.

The two `cause_friction` sites (`0x5A55D0`, `0x5A5791`) run only when no frame
succeeded, and only when the pair still has Truce/Treaty/Pact
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

### `break_treaty` (`0x55F770`)

`break_treaty(faction, target, mask)` asks a human whether to break an agreement. It returns
nonzero to cancel the action. Status is `diplo_status[faction][target]` (`0x96C9F8`). No prompt
and 0 when the pair is at Vendetta (`0x10`, `0x55F798`) or `status & mask` is 0 (`0x55F7A3`).
Otherwise, first match wins:

- Pact (`0x1`, `0x55F86C`): the `#BREAKINGPACT` notice ("abrogate the pact during face-to-face
  negotiations"), and the action is cancelled.
- Treaty (`0x2`, `0x55F894`): `#BREAKINGTREATY`, "Cancel action" / "Break treaty!".
- Truce (`0x4`) without `0x10000` (`0x55F8BE`–`0x55F8C2`): `#BREAKINGTRUCE`, same choices.
- `mask & 0x10000` (`0x55F8EE`): 0. Else `#BEGINVENDETTA`, "Cancel action" / "Pronounce
  Vendetta!".

`battle_fight_2` calls it at `0x507117` with mask `0xB` (Pact | Treaty | Commlink), before the
odds dialog (`0x507CA4`). The Commlink bit means any contacted faction with no agreement gets the
`#BEGINVENDETTA` prompt. Thinker gates the call on a human attacker that is not a probe and not
attacking an artifact off a base, and its `Fix:` passes Truce instead of Commlink, so a faction
with no agreement is attacked without a prompt (`refs/thinker/src/veh_combat.cpp`).

The other callers pass mask 7 (Pact | Treaty | Truce), so a faction with no agreement is never
asked: `sat_attack` (`0x4A050D`), `action_airdrop` (`0x4CC766`), `Console_terraform`
(`0x4D498F`), `Console_destroy` (`0x4D5A06`), `Console_use_ability` (`0x4D8334`) and
`order_veh` (`0x597551`). `break_treaty` only asks: it changes no status itself
(its calls are `parse_says`, `get_pact_hood`, `NetMsg_pop` and `popp`), so the attack that
follows a "Break" answer is what breaks the agreement.

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

- **Portrait** (`DiploPop_draw_portrait` `0x43F790`): mood from the stored
  `diplo_friction[rival][CurrentPlayerFaction]` (`0x43FA1E`–`0x43FA2C`), the rival's friction
  toward the player. Raw: only `diplomacy_caption` floors at Vendetta. Labels `REPUTATION`
  (label 932, `0x43F916`) before the rival's `#REPUTE` (`0x43F955`) and `MOOD` (label 933,
  `0x43F9E8`) before its mood.
- **InfoWin** (`InfoWin_setup_text` `0x456300`): the line type at `0x456D16` draws the cached
  rival's (`DiploFrictionFactionID` `0x93FAC0`) mood from `DiploFriction`. The line type at
  `0x456F13` draws `CurrentPlayerFaction`'s own title and `Integrity` (label 222) with
  `clamp(integrity_blemishes[player], 0, 7)` (`0x457049`–`0x457064`).
- **Intel report** (`ReportWin_draw_intel` `0x4A2210`): `diplomacy_check` (`0x53A980`) at
  `0x4A26CE`, then `MOOD` from the cached `DiploFriction` (`0x4A2749`).

### `reputation` (`0x53A100`)

`int(faction, other)` = `clamp(integrity_blemishes - diplo_gifts[other], 0, 99)`.
AI uses this; the portrait does not.

### `diplomacy_caption` (`0x5399A0`)

`void(a, b)`. Same mood bands on cached `DiploFriction`. If `diplo_status[a][b]` has
`DIPLO_VENDETTA` and friction < 15, it sets the cached `DiploFriction` to 15 (Obstinate) at
`0x5399DE`; the stored friction is untouched. The caption is b's `noun_faction` (with its gender
and number), `" - "` (`0x539A90`), then the mood name through `say_fac_special` (`0x58F700`),
which wraps it as `$<{noun gender}{1|2}:mood>` so a gendered language can inflect it. Label 488
`Submissive` replaces the mood when the band is 0 and `0x93FA98` is set without a Pact
(`0x539A9E`–`0x539AB6`), or when b→a holds a Pact with `DIPLO_HAVE_SURRENDERED`. The result goes
to slot 7 (`parse_says(7, …)` at `0x539B35`), which talk scenes read as `$CAPTION7`.

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
- `battle_fight_2` `0x506AF0`, three sites:
  - **+3 (`0x5078BB`), refused demand.** Right after `act_of_aggression(atk, def)`, for every
    non-probe attack on a non-artifact, each faction `i` (not either side) that owns the
    **attacker's** tile (`MAP+7`): when atk→i holds a Pact, def→i has a commlink and no Vendetta,
    and `def.mil_strength_1 > i.mil_strength_1` (`+0x1060`), `treaty_on(i, def, 0x2000)` (want
    to talk), then `cause_friction(def, i, 3)` only if i→def carries `0x800000`. That flag is
    set only by `communicate`'s `treaty_on(ai, player, 0x800000)` (`0x551368`, `0x55172D`,
    `0x551CF9`) on the refuse branch of `#DEMANDATTACK*`, and `treaty_on` clears it on any status
    change. Thinker's `mod_battle_fight_2` matches.
  - **−1 (`0x50A1AB`), allied ground.** The territory owner `o` of the **defender's** tile
    (`tx, ty`), land only (`alt >= 3`), not either side, with o→def at Vendetta and o→atk
    holding Truce/Treaty/Pact: `diplo_gifts[atk][o]` is raised to at least 1 (`0x50A19C`), then
    `cause_friction(o, atk, -1)`. Runs before kills and captures (`battle_kill` `0x50A8F4`),
    whether or not the attacker survives. A bombard strike returns first (`0x508B4F`), as does a
    `CT_WEAPON_ONLY` single round between a sea and a non-sea unit that leaves both alive
    (`0x50A0F8`–`0x50A118`). An unowned tile reads `Faction[-1]` (no bounds check at
    `0x50A147`); Thinker adds `sq->owner >= 0`.
  - **+5 (`0x50B1EF`), relics.** See "Alien artifacts".
- `action_terraform` `0x4C9B00`: **+5 (`0x4CA3EB`), rainfall.** Only for condenser, raise land
  and lower land (items `0xC`, `0x10`, `0x11`), which re-run `world_rainfall`. While `CurrentTurn`
  is non-zero, the scan keeps a grievant in `0x9B22DC` (`0x5C4DE6`–`0x5C4E36`): it walks changed
  tiles in scan order and stops for good once `0x9B22E0` is set (the first changed tile within 2
  of a `CurrentPlayerFaction` base, that tile still examined). A tile that got **drier** (old >
  new), while the record is unset or AI, records the faction of `base_find(x, y)`: the nearest
  base of any faction, no limit, by `vector_dist`, first base index on ties (`jg` at
  `0x4E3C22`). A human record is never replaced. `action_terraform` then acts when the record
  `v` is AI, the terraformer is human, and terraformer→v holds neither Pact nor Vendetta
  (`test 0x11`, `0x4CA3AD`); no commlink is needed. v→terraformer wanting revenge (`0x20`) gets
  `set_treaty(v, terraformer, SHALL_BETRAY, 1)` (`0x4CA3DD`); otherwise
  `cause_friction(v, terraformer, 5)`. Thinker's `action_terraform` rewrite drops both (its
  TERRAYOURS comment).
- `communicate` `0x54FFD0`: friction −1/−2/+1 on AI-initiated branches, which also write
  `diplo_gifts` (`0x553F0B`, `0x55518E`, `0x5551B3`, `0x555576`, `0x55568D`; not mapped). Its
  blemish decrement at `0x557A99` is the `SELFATROCITY` warning ("Gifts", Also recorded), not a
  gift. Refusing `#DEMANDATTACK*` while the pair holds a Pact adds +4 **inline**
  (`0x551C35`–`0x551C4E`, and the cached `DiploFriction` at `0x551C65`), outside
  `cause_friction`.
- `make_gift` `0x547C10`: the player's gifts ("Gifts", "Base gift").
- `setup_player` `0x5B0E00`: zeros blemishes and `diplo_reputation` (`0x5B1210`).

No per-turn friction decay among the 19 `cause_friction` calls or the inline write above.

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

Scene labels in the script file, not a chain state machine: `INTRONEW0–5`, `INTRO0–3`,
`INTROMAD0–4`, `DIPLOMENU`, `PROPOSALMENU`, `VENDETTA0–18`. `get_patience` `0x53A150` uses raw
cached `DiploFriction` and status, not the mood index. Each faction file's `#FACTIONTRUCE` /
`#FACTIONTREATY` is that AI's own offer.

**Script file.** The default popup file is the string at `0x691B0C` → `SCRIPT` (`0x691B7C`),
`.txt` appended (`0x588AD7`) and set with `BasePop_set_def_file` (`0x6043D0`). Alien Crossfire
labels such as `INTRODUCEENEMYALIEN` (pushed at `0x552153`) exist only in `xscript.txt`, so the
expansion reads `xscript.txt`; the prefix mechanism is not mapped. The third `X_pops` argument
`0x6846D8[faction]` is the rival's portrait sprite (`SpriteBox_sprite`), not a file.

**Slots** (`parse_says` `0x625EC0`, `parse_say` `0x625E50`, `parse_num` `0x625E30`). Ten string
slots at `0x9BB5E8 + 256·n`, each with a gender (`0x9BB5C0[n]`: 0 M, 1 F, 2 N) and plural
(`0x9BB570[n]`) that default to `0x9BBFEC` / `0x9BBFF0` when passed −1. Ten number slots at
`0x9BB598[n]`.

**Markup** (`parse_string(src, dst)` `0x625880`), switched on the character after each `$`:

- `$$`: a literal `$`.
- `$NUMBERn`, `$NUMn` (and `$HEXn`, also decimal): number slot n.
- `$LINK<`: left as written for the renderer.
- `$<spec:f0:f1:…>`: a form. `spec` is a slot digit (that slot's gender and plural), `#n` (plural
  unless number slot n is 1), or `M` / `F` / `N` with an optional `1` / `2` (singular / plural).
  The form chosen depends on how many `:` follow the spec (`0x625BF9`–`0x625C43`): two forms by
  plural; three by gender; four by masculine-or-not and plural (`M sg, F/N sg, M pl, F/N pl`); six
  by gender then plural; any other count takes the last. With no `:` left, the forms come from
  the first slot named after the spec (`$<M1:$FACTIONADJ0>` inflects slot 0's own `a:b` text).
- Anything else, `$WORDn`: string slot n, where n is the first digit after the `$`
  (`findnum` `0x628B30`); the word is only a hint. A `$` with no digit after it ends
  substitution.
- With language `0x9BC054 == 1`, a pass elides ` de ` / ` le ` / ` la ` / `De ` / `Le ` / `La `
  before a vowel or h (`0x625D12`, table `0x6972D4`).

`{…}` is untouched by `parse_string`; the renderer highlights it.

**Faction text** (`read_faction` `0x586090`; Thinker's rewrite matches). After `#KEY`, line 1 is
`formal_name_faction`, `desc_name_faction`, `noun_faction`, noun gender (`M`/`F`/`N`), noun
number (1 or 2), `name_leader`, leader sex (`F` is female), then the AI priorities; lines 2–4 are
bonuses and social priorities; line 5 `adj_name_faction`; line 6 assistant, scientist, city;
line 7 `title_leader`, `adj_leader`, `adj_insult_leader`, `adj_faction`, `adj_insult_faction`;
line 8 `insult_leader`. `get_phrase(f, n)` (`0x58F810`) returns raw line `n + 9`, the n-th line
after `insult_leader`. `get_noun_phrase(f, n)` (`0x58F8B0`) returns that line's first comma item
and sets the default gender and number from its `M1` / `F2` / `N1` tag. `get_pact(f)`
(`0x58F9D0`) is label 201 Pact Brother or 202 Pact Sister by f's leader sex, singular;
`get_pact_hood(a, b)` (`0x58FB00`) is 206 Pact of Sisterhood when both leaders are female, else
205 Pact of Brotherhood, masculine singular. Labels are `labels.txt` `#LABELS`: a count, then
label 0.

The binary names none of the 15 phrase lines. SMAC's faction editor (`facedit.txt` `#SENTENCES`)
labels each with the sentence it completes, and our text packs key them by the names below. In
SMAC's own files, 2–6 and 8 are what others say about or to that faction, 3 and 6 the second-person
forms of 2 and 5, and 8–11 the tagged noun lines.

| n | Name | `facedit.txt` sentence |
|---|---|---|
| 0 | `goal` | Together we shall both ___ |
| 1 | `intent` | My intent is ___ |
| 2 | `agenda` | S/he is bent on ___ |
| 3 | `your_agenda` | You seem bent on ___ |
| 4 | `misdeed` | I shall not stand here while you ___ |
| 5 | `pastime` | S/he spends her/his time ___ |
| 6 | `your_pastime` | You spend your time ___ |
| 7 | `accusation` | I have been accused of ___ |
| 8 | `methods` | Your ___ does not impress me |
| 9 | `projects` | Think how this could benefit your ___ |
| 10 | `pursuits` | I trust your ___ are proceeding to your satisfaction |
| 11 | `tribute` | … it is customary to remit me a small ___ |
| 12 | `service` | … for the services my forces provide in ___ |
| 13 | `enforcers` | I shall instruct ___ to see that no such … |
| 14 | `code` | You are in contravention of ___ |

**Greeting** (`introduce(player, rival)` `0x53D360`, called by `wants_to_speak` at `0x53E240`).
Slots: 0 player `title_leader`, 1 player `name_leader`, 2 rival `title_leader`, 3 rival
`name_leader`, 4 rival `noun_faction`, 8 rival `adj_name_faction`, 9 player `adj_leader`. The label
is `INTRO` + suffix + a random index, from `diplo_status[player][rival]` (`0x53D4F1`):

- `0x8000000` unset, or a multiplayer game with a human rival: `INTRONEW` + `random(6)`
  (`0x53D6D6`), slot 6 `get_phrase(rival, 1)` (`intent`). `INTRONEW2` sets slot 5 to the player's
  `noun_faction` (`0x53D83E`); `INTRONEW3` sets it to "a" / "an" before a rank word indexed by
  `7 - ranking` (`0x53D725`–`0x53D834`).
- Else `DIPLO_WANT_REVENGE`: `INTROMAD` + `random(5)` (`0x53D52D`).
- Else `INTRO` + `random(4)` (`0x53D60B`).

`0x8000000` is set only by `communicate`'s AI-initiated introduction and betrayal branches
(`INTRODUCEENEMY*`, `BETRAYFRIEND`): `treaty_on(…, COMMLINK|0x8000000)` at `0x5522CC`,
`0x5524D1`, `0x552745` and `0x5529C6`. No other immediate in the binary sets it. Thinker's
`whose_territory` requires it beside the commlink. A faction met by contact always greets from
`INTRONEW`.

**Diplomacy menu** (`diplomacy_menu(player, rival)` `0x54C560`). Slots 0 player `name_leader`,
1 `get_pact_hood(player, rival)`, 7 the caption; `DIPLO` is the prompt. Its items are
`DIPLOMENU`'s lines; ids differ from positions (lines 4–8 carry ids 5, 6, 7, 3, 4):

- id 0 finished (line 0): always.
- id 1 trade again on the same terms (line 1): after a trade this session (`0x93FAD0`).
- id 2 make a proposal: line 2, or line 3 "another proposal" once one was made (`0x93FAD4`).
- id 5 battle plans (line 4): single player, `0x93FA94 > 0`.
- id 6 call off a Vendetta against a friend (line 5): single player, `0x93F7DC > 0`.
- id 7 council vote (line 6): `can_call_council`, neither faction with rule flag `0x80`
  (`0x946F58`).
- id 3 renounce the Pact (line 7): `diplo_status[rival][player] & DIPLO_PACT` (`0x54C8A7`).
- id 4 demand withdrawal (line 8): `diplo_status[player][rival] & 7` (`0x54C8F4`).

No item declares Vendetta or ends a Truce or Treaty; those happen at an attack (`break_treaty`).

**Renounce** (`communicate(player, rival)` item 3, `0x5585B2`). `REALLYBROKE` with the menu's slots
still set, so its `$PACTBROTHERORSISTER0` reads the player's name. Item 0 backs down: slot 0
`get_pact_hood`, then `DROIDS`. Item 1 confirms: the player's `integrity_blemishes` `+ 1` inline
(`0x55861B`–`0x55862B`), `net_treaty_off` (`0x558631`), `diplomacy_check` and the caption, slots
0 `get_pact(player)`, 1 `get_title(player)`, 2 `get_name(player)`, `BROKEPACT` (`0x5586A0`),
then `net_pact_ends` (`0x5586AE`).

**Proposal menu** (`proposal_menu(player, rival)` `0x54DCF0`). Slots 0 rival `title_leader`,
1 `get_pact_hood(player, rival)`, number 0 the player's loan balance, 7 the caption; `PROPOSAL`
is the prompt. Items: 0 never mind and 1 gift, always; 2 Pact, when the pair holds a Treaty and
no Pact (`0x54E012`); 3 Treaty, when it holds no Treaty (`0x54E04E`), Vendetta included; 4
research data, unless game rule `0x2000000` (`0x9A649C`). `make_a_proposal` (`0x54F420`) sends
item 2 to `propose_pact(player, rival)` and item 3 to `propose_treaty(player, rival)`, which
calls `make_treaty` on acceptance (`0x545CD2`); `treaty_on` clears `DIPLO_VENDETTA` as it sets
a treaty (`0x55B8D5`). `pledge_truce` is called only from `communicate`'s AI truce offers
(`0x5562C3`, `0x556B53`).

**Replies.** Each maker takes `(listener, speaker)` and pops with the speaker's portrait:

- `pact_of_brotherhood` `0x53CAF0`: `MAKEPACT`, slots 0–2 speaker `title_leader`,
  `name_leader`, `noun_faction`; 3 `get_pact(listener)`; 4 `get_phrase(listener, 0)`;
  5 `get_phrase(speaker, 0)` (both `goal`).
- `make_treaty` `0x53CE10`: `MAKETREATY` + `random(2)`, slots 0–2 listener `title_leader`,
  `name_leader`, `noun_faction`; 3–5 the speaker's; 6 `get_noun_phrase(listener, 9)`;
  7 `get_noun_phrase(speaker, 9)` (both `projects`), which overwrites the caption slot.
- `pledge_truce` `0x53D160`: `MAKETRUCE`, slots 0–2 listener, 3–5 speaker, as above.
- `propose_pact(player, rival)` `0x543BC0`: slots 7 caption, 0 `get_pact(player)`, 1 player
  `title_leader`, 2 player `name_leader`; `REJPACT` adds 3 `get_noun_phrase(rival, 9)`
  (`projects`, `0x544220`).
- `propose_treaty(player, rival)` `0x544E40`: slots 0 player `name_leader`, 1 `title_leader`,
  2 `noun_faction`, 3 `adj_name_faction` (`0x545170`–`0x5451E5`); `REJTREATY` at `0x545310`.

### Base value (`value_of_base` `0x54CB50`)

`int(base_id, faction, other, fear, hostile)`. `faction` is the AI weighing the base and `other`
the human it deals with; all three callers pass (rival, player), and the base may belong to
either. Every halving truncates toward zero.

1. `n = vulnerable(faction, x, y)` (`0x59E980`): the binary's `vector_dist` from `faction`'s
   first base (by index) holding Headquarters (facility 1), or 12 with none. Thinker's rewrite
   matches. `n ≤ 0`, the base being `faction`'s headquarters, returns −1 (`0x54CB96`).
2. **Neighbours** (`0x54CBDB`–`0x54CC90`): `TableOffsetX/Y` (`0x66EFBC` / `0x66F440`, 289
   entries, rings 0–8 in Thinker's `path.h` order) entries 1–168, which are rings 1–6, with x
   wrapping unless flat and rows off the map skipped. Region means the tile's `MAP+3` against the
   valued base's. At each tile holding a base, first match wins:
   - `faction`'s base: `n −= 1` when it is in the same region; the scan stops either way;
   - another region: skip;
   - `other`'s base: `n += 1`;
   - a third faction's base: the scan stops.
3. `v = (max(credits[faction], credits[other]) + 1000) / (n + 4) × max(pop_size, 3)`, then at
   least 100 (`0x54CD33`–`0x54CD4D`). Credits are `energy_credits` (`Faction+0x220`).
4. Halve when no unit stands in the base: `items & BIT_VEH_IN_TILE` (`0x2`) clear, or the `val2`
   low nibble (occupant's faction) ≥ 8 (`0x54CD7A`–`0x54CD96`).
5. `F` and `O` are `region_total_bases[region]` ("Regions") of `faction` and `other`
   (`0x54CD98`–`0x54CDFB`):
   - `O == 0`: ×2;
   - `F == 1` and `O ≤ 1`: ×2;
   - `hostile`: ×4;
   - `O > 0` and `F > 0`: halve when `O ≥ 5F`, and halve again when `fear`.
6. Facilities 1–64 in id order (`0x54CDFD`–`0x54CE5C`): the k-th one built adds
   `k × Facility.cost` (rows, `+0xC`). The orbital ids 65–68 are outside the loop.
7. Each secret project whose `SecretProjects` (`0x9A6514`) entry is this base adds `25 × cost`
   (`0x54CE5E`–`0x54CE88`).
8. Each unit of `faction` in the base's stack adds `2 × UNIT.cost` (rows, `+0x29`)
   (`0x54CE8A`–`0x54CEF2`).
9. ×2 when `faction`'s `MFaction.filename` is `BELIEVE` (`strcmp` `0x645660`,
   `0x54CEF7`–`0x54CF1B`).
10. ×4 when `is_objective(base)` (`0x5AC060`): a scenario victory rule bit or the base's
    objective event flag, never set in a normal game.

The callers pass `fear` = `DiploFearScale` (`0x93FA98`) and `hostile` = `DiploPrevHostility`
(`0x93FA68`) or `DiploAttackChance` (`0x93FAA0`) non-zero. `diplomacy_check` (`0x53A980`) fills
all three for the pair; it is not mapped. One fear term is `8 × pop_size` for each of the rival's
ungarrisoned bases next to the player's units (`0x53B0FE`–`0x53B10D`). `DiploFearScale` is the
`0x93FA98` that `diplomacy_caption` reads.

### Base swap (`base_swap(player, rival)` `0x54D410`)

`make_a_proposal` calls it (`0x54F77E`). `DiploAskBaseSwapID` (`0x93FA7C`) is the rival's base
the player asks for; `DiploBidBaseSwapID` (`0x93FA30`) is the player's base offered, or −1.
`DiploCounterProposal` (`0x93FAB0`) is the `COUNTERMENU` id the player offered.

- `ask = value_of_base(asked, rival, player, DiploFearScale, hostile)` and
  `bid = value_of_base(offered, rival, player, 0, 0)`.
- `ask` halves when the asked base's `faction_id_former` (`BASE+0x5`) is the player, and again
  when rival→player holds Pact with `DIPLO_HAVE_SURRENDERED` (`0x2000000`). `bid` doubles when
  the offered base's former owner is the rival.
- Refusals, in order, each returning 1:
  - an offered base with `bid < 0`, or a player with one base (`base_count` `Faction+0x105C`):
    `NOHQ` ("You cannot offer to vacate your headquarters or your last base."). The offered
    base's own Headquarters is not tested;
  - an asked base with `ask < 0` or Headquarters, or a rival with one base: `NOMYHQ`;
  - rival→player `DIPLO_WANT_REVENGE` (`0x20`), unless the counter is 3 (threaten):
    `NEVERBASESWAP`.
- **Base for base** (`0x54D688`–`0x54D6AB`): `game_random(0, 100) + bid` against
  `game_random(0, 100) + ask`. A higher **ask** swaps: `give_a_base(asked, player)`,
  `give_a_base(offered, rival)`, `YESBASESWAP`. Otherwise `NOBASESWAP3`, which calls the asked
  base gilded spires and the offered one a pile of pustulent filth. The comparison contradicts
  that text: the AI gives up a base it values more than the one it gets.
- **Base for energy** (no offered base): price =
  `ask × (ranking[player] + ranking[rival]) / (2 × ranking[rival])` (`Faction+0x4`), doubled when
  the player is `FactionRankings[7]` (`0x9A6508`, the top rank) (`0x54D801`–`0x54D859`).
  - Counter 5 (energy) or 2 (name a price) with the player's credits ≥ price: `PAYBASESWAP`
    with `$NUM0`; on payment `give_a_base(asked, player)`, `BASEOK`.
  - Counter 5 short of the price, or a counter other than 2 and 4: `NOBASESWAP` with the price.
  - Counter 2 short of the price falls through to research.
- **Base for research** (counter 4, or 2 short): `sum` is `tech_val(t, rival, 1)` over techs the
  player has and the rival lacks. `NOBASESWAP2` when `tech_ranking[rival] / 2 >
  tech_ranking[player] / 2 + 2` (`Faction+0x35C`) or `(turn + 7) / 8 × sum < ask`. Otherwise
  `TECHBASESWAP`; on acceptance the rival gets each such tech, then `give_a_base(asked, player)`
  and `BASEOK`.

Thinker replaces it (`mod_base_swap`, `base_trade_value` in `gui_dialog.cpp`) with its own
valuation. A base for a base swaps only when the offered value is at least the asked one, and a
multiplayer game or a human rival refuses unless the rival surrendered.

### Gifts (`make_gift` `0x547C10`)

`make_a_proposal(player, ai)` (`0x54F420`) sends proposal item 1 ("a small token of my goodwill")
to `make_gift(player, ai)` (`0x54F81A`). `proposal_menu` first asks `COUNTER1` ("You mean a gift?
I like gifts.") over `COUNTERMENU` and stores the chosen id in `DiploCounterProposal`
(`0x93FAB0`, `0x54EE5D`). For a gift the menu offers: id 0 never mind; id 4 some research data,
unless game rule `0x2000000` (`0x54ED59`); id 7 all research data (`0x54ED96`); id 5 energy
credits (`0x54EDD0`); id 8 a base, when the player has more than one base in single player
(`0x54EE18`–`0x54EE35`). `make_gift` branches on the id: 4 or 7 tech, 5 energy, anything else
base ("Base gift"). Below, giver is the player and recipient the AI; "key" is
`MFaction.search_key` (`+0x1C`).

**Energy** (`0x547C70`):
- `OFFERENERGY` lists amounts: the giver's credits rounded down to a multiple of 25, then halved
  and rounded down to 25 again, while non-zero (`0x547CD4`–`0x547D39`).
- The value is the amount, doubled when the giver's key is `MORGAN`. A `SPARTANS` giver's value
  is doubled and then halved, so it is unchanged (`0x547DDE`–`0x547E49`).
- Size, from credits read before the transfer:
  - **generous** when value ≥ recipient's credits / 2, ≥ giver's credits / 3 and ≥ 2 ×
    `CurrentTurn` (`0x547E62`–`0x547E9E`);
  - **notable** when value ≥ recipient's credits / 4, ≥ 25, ≥ giver's credits / 6 and ≥
    `CurrentTurn` (`0x547EA7`–`0x547EE8`);
  - otherwise small.
- The credits move (`0x547F35`–`0x547F46`).
- Friction (`0x547F6B`–`0x548025`): units = value / 5; sum = min(units, 10) / 2, plus each
  further block of up to 20 units divided by 3, then 4, then 5, and so on. The recipient's
  friction toward the giver changes by −trunc(3 × ceil(sum / 5) / 2), clamped to 0–20 and
  mirrored into the cached `DiploFriction`: `cause_friction` written inline. Under 10 credits
  0; 10–25 → −1; 100 → −3; 200 → −4; 500 → −7.
- Reply `GAVEENERGY`. A generous gift says `GAVEENERGY1`, or `GAVEENERGY2` ("I may have to
  rethink my opinion of you!") when recipient→giver carries `0x60` or `great_satan(giver, 0)`,
  and calls `treaty_off(giver, recipient, 0x60)` (`0x5480BD`).

**Technology** (`0x5488D2`):
- `tech_analysis(giver, recipient)` picks the tech the AI wants (`0x93FAA8`) and a second one
  (`0x93FA18`). With none, `REJTECHNOTHING`.
- Id 4: `OFFERTECH` names the first tech; `OFFERTECH2` also lets the player offer the second.
  Value = `tech_val(tech, recipient, 1)`, taken before `tech_achieved` (`0x548AFD`).
- Id 7: in id order, every tech the giver has and the recipient lacks adds its `tech_val` and is
  then delivered, so later values see earlier deliveries (`0x548984`–`0x548A00`).
- The value doubles when the recipient's key is `UNIV` or `SPARTANS`, then halves when it is
  `BELIEVE` (`0x548B50`–`0x548BA9`).
- Friction: the recipient's toward the giver changes by −value, clamped (`0x548BAC`–`0x548BF6`).
- **Generous** when value ≥ 16 (`0x548C14`): reply `GAVETECH1`, or `GAVETECH2` when
  recipient→giver carries `0x60` and the cached `DiploFriction` is non-zero, and
  `treaty_off(recipient, giver, 0x60)`. Otherwise `GAVETECH`, and the gift counts as notable.

**Gift count** (`diplo_gifts[giver][recipient]`, `Faction+0x12C`):
- Generous: +1 if the count is below the giver's blemishes (`0x5480EF`), then +1 again if it is
  still below them, or `great_satan(giver, 0)`, or multiplayer (`0x548124`–`0x548149`).
- Notable: the second step, only when the count is 0. Small: no change.
- In single player without the great satan the count never passes the giver's blemishes.
  `reputation(f, other)` reads it as `clamp(blemishes - diplo_gifts[other], 0, 99)`.

**`tech_val(tech, faction, 1)`** (`0x5BCBE0`), the simple path; Thinker's `mod_tech_val`
asserts it matches:
1. value = Σ weight × (priority + 1) over power, tech, wealth and growth. Weights are the tech's
   `#TECHNOLOGY` columns 3–6 (power, tech, wealth, growth); priorities are the faction's 0/1 AI
   priorities.
2. Halve, rounding up (`0x5BCE42`), unless one matches: the tech has power and the faction has
   the power priority or any Vendetta; tech and tech; wealth and wealth; the tech has growth and
   the faction has the growth priority or fewer than 4 bases.
3. For an AI faction that lacks the tech: add 2 − 2 × owners when more than one faction owns it
   (`0x5BCEC0`); when the tech's level is below the highest level the faction knows (at least
   1), scale by (level + 1) / (highest + 1); floor at 1. A tech's level is 1 plus its deepest
   prerequisite's (`tech_recurse`).

**Also recorded:**
- `great_satan(f, 0)` (`0x539C00`) holds only when f is human, ranked first, and has more than
  max(4, (turn + 25) / 50) bases; difficulty and turn gates pass; and either the climactic
  battle makes f the target, or f's score beats the second faction's by a difficulty ratio.
- `treaty_off(a, b, mask)` (`0x55B760`) clears mask both ways. A mask with `0x7` also clears
  `0x1100000`, `0x10` also clears `0x8040`, and `0x20` also clears agenda bits `0xC00`.
- `SELFATROCITY` (`0x557A59`): when the player's `atrocities` (`Faction+0x1D4`) is non-zero and
  no faction is its atrocity victim, the AI warns it once per pair (gated by `0x200000` /
  `0x400000`). The atrocity count and blemishes each drop by 1 while above 1 (`0x557A99`).
- `tech_trade` writes friction after a swap (`0x542034`, not mapped). `energy_trade` writes
  none.

### Base gift (`make_gift` `0x548260`)

Gift id 8 offers `DiploBidBaseSwapID`.

- `v = value_of_base(offered, rival, player, DiploFearScale, hostile)`. The rival's friction
  toward the player becomes `clamp(friction − trunc(3v / 2), 0, 20)`, and the cached
  `DiploFriction` takes the unclamped delta (`0x54829D`–`0x5482FE`).
- Then `v` doubles when the player's `search_key` (`MFaction+0x1C`) is `SPARTANS`
  (`0x548304`–`0x54832D`).
- **Generous** when `v ≥` the rival's credits / 2, `≥` the player's credits / 3 and `≥ 2 ×`
  `CurrentTurn` (`0x54832F`–`0x548391`), or always when the player's `filename` is `BELIEVE`
  (`0x548398`–`0x5483B3`). A generous gift calls `treaty_off(player, rival, 0x60)` and adds 1 to
  `diplo_gifts[player][rival]` while it is below the player's blemishes (`0x548423`–`0x5484C4`).
- Every base gift then adds 1 more while the count is below the blemishes, or when
  `great_satan(player, 0)` or multiplayer (`0x5484CB`–`0x548516`).
- `give_a_base(offered, rival)`; the rival's `diplo_patience` toward the player drops by 2, or 3
  when generous (`0x5485B6`–`0x5485E0`).
- The reply is plain `BASEGIFT` (`0x68DC1C`, `0x5485D9`). The label built in the local buffer,
  `BASEGIFT1`, or `BASEGIFT2` when rival→player carries `0x60` or `great_satan(player, 0)`
  (`0x548403`–`0x548452`), is never shown. The tech branch does pop its buffer (`0x548D45`).

### Base transfer and Headquarters

- `give_a_base(base_id, faction)` (`0x54CF40`), used by gifts, base swaps and cession, never
  calls `capture_base` and never addresses `BASE.facilities_built` (`+0x8C`). Every facility goes
  with the base, Headquarters included: a given Headquarters leaves the giver with none and the
  receiver with two. `find_hq` and `vulnerable` then take the receiver's first by base index.
  The function does reset the queue (`queue_size` 0, `production_id_last` and `queue_items[0]`
  −`FAC_STOCKPILE_ENERGY`, `0x54D2F0`–`0x54D305`) and zero `minerals_accumulated` (`0x54D2AC`).
- `capture_base` (`0x50C510`) always clears it: `set_fac(FAC_HEADQUARTERS, base, 0)` at
  `0x50D056`, after the random facility loss. Thinker's `mod_capture_base` matches.
- Before that, the **escape** (`0x50C98B`–`0x50CC94`). It runs when the captured base holds the
  Headquarters and its owner has at least 1000 credits (`cmp …, 0x3E8`, `0x50C9BC`). Each other
  base of the owner, by index, scores `score = 1 + Σ vector_dist` to every base of the owner but
  itself and the captured one, each distance doubled when that base is in another region
  (`0x50CB13`), and then `(score << 8) / (region_total_bases[owner][region] + 2 × pop_size)`
  (`0x50CB46`–`0x50CB5B`). The lowest wins, the first on ties (`jge`); the best starts at 9999
  (`0x50C9D7`), so a base scoring 9999 or more is never chosen, and the result must be above 0
  (`jle` at `0x50CB95`), so base index 0 never is either.
  - In single player a human owner is asked (`X_pop("ESCAPE")`, `0x50CBE5`: "Abandon HQ." or
    "Evacuate HQ to …"); an AI owner, and every owner in multiplayer, always moves.
  - Moving costs 1000 (`add …, −1000`, `0x50CC00`), sets the Headquarters on the chosen base and
    clears it here. The local human then sees `ESCAPED` when the owner is someone else, or
    `ESCAPED2` in multiplayer when it is the human.

### Base transfer and units

`give_a_base` (`0x54D037`–`0x54D295`) walks every unit of the old owner, last first, and takes
those on the base tile, or on a land tile next to it (`vector_dist ≤ 1`, land only). Diplomatic
status is never read, and other factions' units are left alone.

- A defense or recon plan unit (plan 2 or 3) on the base tile, and a sea unit, changes sides
  (`0x54D20F`): it joins the receiver, is homed at the base, has its moves reset and its order
  cleared unless it is 2.
- Any other unit, and every unit on a neighbouring tile, goes with its whole stack
  (`stack_veh`, `stack_put`) to the old owner's base nearest the given one (`base_find_2`, with
  the given base hidden from the search). Its order is cleared and, if it was homed at the given
  base, it is homed at that base instead. With no such base it changes sides as above.

`capture_base` (`0x50D18E`–`0x50D25A`) re-homes the loser's units homed at the captured base to
the loser's base nearest it (`base_find_2`), and kills one (`kill` `0x5C0B00`) when the loser has
no base left or the unit stands on the captured tile. Units on the tile are not otherwise moved:
conquest needs an empty base, and mind control converts its garrison before this runs.

### Threatened cession (`threaten(player, rival)` `0x549930`)

A base demand backed by a threat values the asked base as
`value_of_base(asked, rival, player, DiploFearScale, hostile)` (`0x54A685`). When
`0 < v < 25 × DiploFearScale` (`0x54A68D`–`0x54A6A3`) the rival cedes it:
`give_a_base(asked, player)`, `BASEOK`, and unless rival→player has `DIPLO_HAVE_SURRENDERED`,
`set_treaty(rival, player, DIPLO_WANT_REVENGE, 1)` (`0x55BB30`). Anything else takes the
threat's refusal path (`0x54A7F6`, not mapped). Thinker's `mod_threaten` may end a Pact first,
then calls the original.

### AI (not fully mapped)

`propose_pact` `0x543BC0`, `propose_treaty` `0x544E40`, `wants_to_attack`
`0x55BC80`, `enemy_diplomacy` `0x55F930`, `great_satan` `0x539C00`. They read
friction, `reputation()`, `DiploGoodwill` `0x93FA9C`, status, ranking.
