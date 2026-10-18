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

### `base_growth` (`0x4EF1C0`)

At the population limit it checks Hab Complex and Habitation Dome directly with `has_fac`
(`0x421670`; facility 24 at `0x4EF448`, 25 at `0x4EF479`). A Hab Complex or Dome finished in
`base_production` the same turn therefore lets the base grow that turn.
