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
