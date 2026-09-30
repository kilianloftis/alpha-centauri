---
name: Ecological damage
overview: "Per-base eco-damage score assembled from tile/base/faction effect contributions and a Lua formula in `config/eco_damage.lua`, a faction-wide clean-minerals cap raised by fungal blooms and eco facilities, and a per-faction `EcoDamage` turn stage that rolls the score as a fungal-pop percentage and applies the `FungalBloom` effect on a hit."
todos:
  - id: stats
    content: Add EcoDamageContribution / EcoDamageWorkedContribution / EcoTerraformScale / EcoCleanMinerals / EcoDamageReduction / EcoMineralOffset to StatId_t, ParseStatId, KindFor, DomainFor
    status: completed
  - id: config
    content: Add config/eco_damage.json + config/eco_damage.lua, EcoDamageConfig_t/parser, EffectSourceKind_t::EcoDamage, GameDataPaths + LoadGameData
    status: completed
  - id: contributions
    content: Author eco contributions on improvements.json (Borehole/Mirror/Condenser/Forest/Farm/Mine/Solar/SoilEnricher/KelpFarm/Base-on-water)
    status: completed
  - id: calculator
    content: EcoDamageCalculator (Lua bridge) + EcoDamageInputs_t; BaseManager::GetEcologicalDamage() memoized on the base effects revision
    status: completed
  - id: faction-state
    content: "EcologyLedger on GameState (blooms, clean-mineral grants, virtual minerals, revision); GrantCleanMinerals + AddVirtualMinerals triggered effects; entries on the four eco facilities and Tectonic_Payload"
    status: completed
  - id: stage
    content: EcoDamage per-faction turn stage after WorldEvents; roll + pop-tile pick + EvFungalBloom + player notice; records the bloom in EcologyLedger; on_pop_effects applies FungalBloom with the tile stamped
    status: completed
  - id: eco-scale
    content: "Eco multiplier emitters: planet levels in social_rating_effects.json, config/native_life_levels.json + GameRulesConfig.nativeLifeLevelId + FactionEffectsPool"
    status: completed
  - id: world-events
    content: config/world_events.json registry with a Cycle trigger, active-event state on GameState, WorldEvents starts/expires events, CollectWorldExtras serves active effects; Perihelion is the first entry
    status: completed
  - id: docs
    content: docs/architecture/ecology-system.md; update high-level, turn-system, effects-system, difficulty-system, game-rules/turn-structure
    status: completed
  - id: tests
    content: Parser, calculator, contribution-routing, cap, stage-roll tests; ./bd test
    status: completed
isProject: false
---

# Ecological damage

## Sources

Four write-ups of the SMACX formula, in agreement on every term used below:

- Apolyton Column #175, *SMACX Eco-Damage Formula Revised* — the primary reverse-engineered
  breakdown.
- Alpha Centauri Wiki, *Ecology (Revised)* (alphacentauri2.info / miraheze mirror).
- Alpha Centauri Wiki, *Ecology (Advanced)* — the only source that enumerates the counted
  improvement list outright; it settles what the others leave implicit.
- CivFanatics, *Ecodamage for intermediate players*.

Where they disagree or fall silent, this plan puts the number in config and records the open
question under [Rules decisions needed](#rules-decisions-needed) rather than guessing.

## The rules (interpreted)

Eco-damage is **per base**. The clean-minerals cap it is measured against is **faction-wide**,
and every base gets the whole cap — the cap is not divided between bases.

```text
Terraform     = (Σ tile contributions in the base radius) / 8, scaled by Tree Farm / Hybrid Forest
Cleanmins     = 16 + fungal blooms + eco facilities built since the first bloom
Cleanmins1    = Terraform < 0 ? 0 : min(Cleanmins, Terraform)
Cleanmins2    = Cleanmins - Cleanmins1
DamageFactor  = floor( (Terraform - Cleanmins1)
                       + (Minerals - Cleanmins2 + VirtualMinerals) / (1 + Goodfacs) )  -- floored at 0
EcoDamage%    = DamageFactor * Perihelion * Techs * Life * Difficulty * max(1, 3 - PLANET) / 300
```

The result is a **percentage chance of a fungal pop inside the base radius next turn** — it is
the red number on the base screen. There is no separate threshold constant: `DamageFactor`
floors at zero, so a base under its cap rolls 0%.

| Term | Meaning |
|---|---|
| tile contribution | per-improvement weight, doubled on tiles this base works |
| Tree Farm / Hybrid Forest | halve / zero the terraform term |
| Minerals | this base's mineral production after multipliers, less minerals received from orbit — the subtraction is `EcoMineralOffset`, authored on the orbital building rather than inferred in C++ |
| VirtualMinerals | eco damage arriving from something other than production. Three contributors, summed — see [Virtual minerals](#virtual-minerals-damage-that-is-not-production). SMAC's `5 × Atrocities` is one of them |
| Goodfacs | Centauri Preserve + Temple of Planet + Nanoreplicator in this base, + Pholus Mutagen + Singularity Inductor owned |
| Techs | techs discovered by this faction |
| Life | 1 / 2 / 3 for Rare / Normal / Abundant native life |
| Difficulty | 3 on Librarian and below, 5 on Thinker and Transcend |
| PLANET | the faction's Planet social rating; `3 - PLANET` clamped to a minimum of 1 |
| Perihelion | 2 while Alpha Prime is at perihelion — 20 years in every 80 — else 1 |

SMAC weights the terraform sum as `2 × worked improvements (kelp farms excepted) + 1 × unworked
improvements + 8 × boreholes + 6 × echelon mirrors + 4 × condensers + 1 if a sea base −
1 × forests`, all over 8. Every one of those numbers becomes an authored effect or a config key
below; none of them is C++.

**The counted set is closed**, and *Ecology (Advanced)* enumerates it:

> For each base total the number of Mines, Solar Collectors, Farms, Soil Enrichers, Roads, Mag
> Tubes, Condensers, Mirrors, and Boreholes. Items in squares which are actually being worked
> count double. Add an extra +8 for each Borehole, +6 for each Mirror, and +4 for each
> Condenser. Subtract 1 for each Forest. Halve if base has Tree Farm, and Eliminate if also has
> Hybrid Forest.

So **Roads and Mag Tubes do count**, Soil Enrichers count, and **Sensors, Bunkers and Airbases
do not**. And the sum is over *improvements*, not squares — "total the number of Mines, Solar
Collectors, Farms, …", the same per-improvement phrasing the Apolyton column uses. Including
Roads in that list is only meaningful under per-improvement counting: a road shares a tile with
a farm or mine on almost every worked square, so counting per square would make its presence in
the list inert.

## Already in the repo

- `StatId_t::EcologicalDamage` exists — **RawScaled**, Base domain, wire form
  `ecological_damage`. `config/difficulty.json` already emits `MultiplyGeometric 3` on
  citizen/specialist/talent/librarian and `5` on thinker/transcend, which is exactly the
  Difficulty term. `DifficultyTests.cpp` pins `ResolveBaseStat(..., EcologicalDamage, 1.0) == 3`.
  This plan keeps that meaning: the stat is the **scale applied to the computed score**,
  resolved with seed `1.0` and handed to the formula as one variable.
- `SocialRatingId_t::Planet` exists and `SocialEngineeringManager::GetSocialRating` returns its
  level. `config/social_rating_effects.json`'s `planet` levels are empty arrays — fine, eco reads
  the numeric level, not effects hung off it.
- `Borehole` / `Mirror` / `Condenser` / `Forest` / `Farm` / `Mine` / `SolarCollector` /
  `SoilEnricher` / `KelpFarm` all exist in `config/improvements.json` with `ThisTile` effects.
- `WorkerAssignmentManager::GetWorkableTiles()` is the base radius;
  `IsTileWorkedByThisBase(pTile)` is the worked test. `CollectTileEffects(tile)` is the
  radius-0 tile effect collector.
- **Supply crawlers are implemented** and already draw the line SMAC's terraform sum needs.
  `Unit::TryStartSupplyCrawl` mints a `WorkedTileClaim` on the unit through the same world
  `WorkedTileIndex` the pops use, and yield is collected separately via `HomeBaseIndex`.
  `IsTileWorkedByThisBase` scans **this base's pops only**, so it returns false for a crawled
  tile — which is exactly SMAC's "worked (not crawled)" exclusion, for free.
- `LuaRuntime` + the `tech_cost.lua` pattern (a `.lua` file returning a named formula string,
  with its own `GameDataPaths` entry) is the model for the score formula.
- **Stored per-faction tallies are one ledger per concern.** `AtrocityLedger` and
  `MindControlLedger` are both owned by `GameState`, keyed by `FactionId_t`, and written by a
  concern-specific triggered effect (`CommitAtrocity`, `RecordMindControl`) whose dispatch arm
  is per-faction-subject — it credits each faction in `context.factions`, so in a probe mission
  or a detonation that is the actor. Eco follows the same pattern with an `EcologyLedger`; see
  [The ecology ledger](#the-ecology-ledger).

## Design

### 1. Score

Six new stats, one existing. Each is a distinct quantity, so each is its own `StatId_t`.

| Stat | Wire form | Kind | Domain | Emitters |
|---|---|---|---|---|
| `EcoDamageContribution` | `eco_damage_contribution` | Additive | Tile | improvements: Borehole +9, Mirror +7, Condenser +5, Farm/Mine/SolarCollector/SoilEnricher/**Road**/**MagTube** +1, KelpFarm +1, Forest −1, Base +1 when `TargetTileHas: Water`. Sensor, Bunker, Airbase, MiningPlatform, TidalHarness author nothing |
| `EcoDamageWorkedContribution` | `eco_damage_worked_contribution` | Additive | Tile | the extra weight counted only while this base works the tile: +1 on every counted improvement **except** KelpFarm and Forest — including Road and MagTube, which the counted set names |
| `EcoTerraformScale` | `eco_terraform_scale` | PureMultiplier | Base | Tree Farm `MultiplyGeometric 0.5`, Hybrid Forest `MultiplyGeometric 0` |
| `EcoCleanMinerals` | `eco_clean_minerals` | Additive | Faction | `eco_damage.json` `effects`: `FactionGlobal Add 16` |
| `EcoDamageReduction` | `eco_damage_reduction` | Additive | Base | Centauri Preserve / Temple of Planet / Nanoreplicator `ThisBase Add 1`; Pholus Mutagen / Singularity Inductor `AllOwnerBases Add 1` |
| `EcoMineralOffset` | `eco_mineral_offset` | Additive | Base | minerals that must not answer to ecology. `Nessus_Mining_Station` emits `AllOwnerBases Add -1` beside its existing `minerals +1` |
| `EcologicalDamage` *(exists)* | `ecological_damage` | RawScaled | Base | the whole multiplier stack — difficulty, Planet rating, native life, perihelion (see [The multiplier is one stack](#the-multiplier-is-one-stack)) |

Splitting contribution into an unconditional and a worked-only stat is what makes SMAC's
"worked improvements count double, kelp farms excepted" expressible as data: a borehole is
`9 + 9` worked and `9` idle, a kelp farm is `1` either way, a forest is `−1` either way.

Because the weights are per improvement and a tile resolve sums every improvement on the tile,
**stacked improvements stack their weight** — a worked tile holding a Road and a Mine
contributes `4`, and adding a Mag Tube makes it `6`. That is the sourced rule, not a side
effect of the data model: the counted set names Roads and Mag Tubes, which share tiles with
farms and mines as a matter of course.

`EcoDamageCalculator` is a thin Lua bridge, the same shape as `TechCostCalculator` and
`PopCompositionCalculator`: it owns no numbers. `EcoDamageInputs_t`:

```cpp
struct EcoDamageInputs_t
{
    double terraformRaw = 0.0;     // Σ per-tile contributions over the base radius
    double terraformScale = 1.0;   // resolved EcoTerraformScale
    int minerals = 0;              // GetMineralProduction outright
    int mineralOffset = 0;         // resolved EcoMineralOffset (negative for orbital)
    int cleanMinerals = 0;         // resolved EcoCleanMinerals (the base 16)
    int fungalBlooms = 0;          // EcologyLedger::FungalBlooms
    int cleanMineralGrants = 0;    // EcologyLedger::CleanMineralGrants
    int virtualMinerals = 0;       // AtrocityLedger::EcoVirtualMinerals + EcologyLedger::VirtualMinerals
    int damageReduction = 0;       // resolved EcoDamageReduction
    int techs = 0;
    double ecoScale = 1.0;         // ResolveBaseStat(EcologicalDamage, 1.0)
};
```

`Calculate` evaluates `damage_formula` from `config/eco_damage.lua` with those as Lua globals and
returns the percentage. A non-finite or negative result throws — a broken mod formula fails
loudly, the way `TechCostCalculator` rejects a non-positive cost.

`BaseManager::GetEcologicalDamage()` assembles the inputs and calls the calculator. The terraform
sum walks `GetWorkableTiles()`, resolving `EcoDamageContribution` on every tile and
`EcoDamageWorkedContribution` additionally on tiles `IsTileWorkedByThisBase` accepts.

`IsTileWorkedByThisBase` is the **required** predicate here, not merely a convenient one: it
scans this base's pops, so a tile held by a supply crawler is not worked by it, and a crawled
improvement therefore contributes its unworked weight only. That is SMAC's "2 × worked (**not
crawled**) improvements" rule, and it holds without a crawler check in the eco code. Asking
`IsTileAssigned` instead would silently double every crawled improvement — and also every tile a
neighbouring or enemy base works — so the walk must not drift onto it.

The base's own tile is outside `GetWorkableTiles()` but still carries improvements — the sea-base
term rides `Base` — so the walk adds it too, at its unworked weight: no pop works it.

The score lives in `BaseEcology`, which `BaseManager` owns and `GetEcologicalDamage()` reads. It
is memoized against the faction's composed effects version, its research revision, the base's
population and mood revisions, the base map's worked-tile and appearance revisions,
`AtrocityLedger::GetRevision()` and `EcologyLedger::GetRevision()` — the same validation shape
`CommerceManager` uses. The two ledger revisions are there for the same reason commerce folds in
the atrocity revision: **an atrocity, a bloom, a clean-mineral grant and
a tectonic strike all move the eco score without touching any effect pool**, so without them the
base would keep serving the old score for the rest of the turn. Crawl start and stop both go
through `WorkedTileIndex`, so they bump the worked-tile revision and invalidate the memo like any
worker reassignment.

### 2. Threshold

There is no separate threshold check. The score **is** the percentage. A new per-faction turn
stage `EcoDamage` sits **after** `WorldEvents` and, for each of the faction's bases, rolls a
uniform `0..99` against `min(max_chance_percent, score)`.

It is its own stage rather than work inside `WorldEvents` because the roll is per faction per
base and `WorldEvents` is `repeatForEachFaction: false` — putting it there means hand-rolling a
faction loop next to the machinery that already does it. `WorldEvents` keeps its own job:
rolling the world events that belong to nobody, Perihelion among them.

It runs after `WorldEvents` specifically so the world's state for the turn is settled before
anybody's score is computed. A Perihelion that starts this turn doubles this turn's eco damage,
rather than landing a turn late — which matters, because the event is the single largest term in
the stack.

`config/turn_stages.json` gains the entry between `WorldEvents` and `VictoryConditionChecks`,
with a description recording both constraints: after `Population` so the turn's composition is
settled, and after `WorldEvents` so active world events are current.

### 3. Outcome

No longer a stub: the `FungalBloom` triggered effect exists, so `on_pop_effects` ships with a
real entry. On a successful roll the stage:

1. picks the **pop tile** from the base's radius and stamps it as `pTile`,
2. calls `EcologyLedger::RecordFungalBloom(owner)` — the `+1` to `Cleanmins` and the gate that
   starts crediting eco-facility grants,
3. emits `EvFungalBloom { factionId, baseId }` on the `EventBus`,
4. enqueues a `PlayerInteractionQueue` notice (`PauseOnEventId_t::FungalBloom`) when the base
   is the player's,
5. applies `eco_damage.json`'s `on_pop_effects` through `ApplyTriggeredEffects` with base,
   faction **and tile** stamped.

The bloom is recorded in C++ rather than by an authored entry: the stage is its only writer, so
a `RecordFungalBloom` effect type would be config surface with nothing else to use it.

`FungalBloom` does the rest of the work already, and more than this plan originally scoped for
it: `ApplyFungalBloom` converts the origin plus a random sample of its Chebyshev-1 neighbours
(skipping bases and tiles that already have fungus), setting fungus **notifies the tile, which
drops the improvements that cannot coexist with it**, and then spawns a uniform draw of native
lifeforms onto the new tiles, owned by the session's native-life faction. Planting fungus,
destroying improvements and releasing mind worms are therefore all covered by one authored
entry — none of them needs code here.

> **Trap:** `FungalBloom_` returns false when the context has no `pTile`, so an `on_pop_effects`
> list applied with only base and faction stamped would silently plant nothing, while the
> ledger has already counted the bloom. Stamping the tile is the one thing the eco stage must
> get right.

Two magnitude sources are authored on `FungalBloom`, and the eco pop must use the literal one:
`tiles_stat` resolves off a **subject unit** (that is how a fungal payload takes its size from
the reactor) and there is no unit in an eco pop, so `tiles` carries the count.

### Virtual minerals: damage that is not production

SMAC's `+5 × Atrocities` is not really an atrocity rule. It is the one place the formula admits
**eco damage that no mineral and no terraformer produced**, expressed as minerals the base is
charged for but never received. Atrocities are its only shipping source, but nothing about the
channel is atrocity-specific, and a tectonic strike is the obvious second customer.

Expressing it as *virtual minerals* rather than as a bolt-on to the final score is load-bearing:
it sits inside the mineral term, so it is divided by `(1 + Goodfacs)` and offset by
`Cleanmins2` like real minerals. A Centauri Preserve therefore mitigates a tectonic scar exactly
as it mitigates a foundry. Adding it to the score instead would make good facilities useless
against precisely the damage they should answer for.

Three contributors sum into the one `virtual_minerals` input, with different lifetimes:

| Source | Channel | Lifetime |
|---|---|---|
| Atrocity records | `AtrocityLedger::EcoVirtualMinerals(perpetrator, rConfig)` | permanent, `bCounted`-gated at commission |
| One-shot events (a tectonic detonation, nerve gas) | new triggered effect `AddVirtualMinerals`, writing `EcologyLedger` | permanent, ungated |
| Standing sources (a facility that is simply dirty) | the `EcoMineralOffset` stat, **positive** | continuous, vanishes with its emitter |

`EcoMineralOffset` is signed and that is deliberate — it is the same quantity in both
directions. Nessus Mining Station emits −1 because orbital minerals must not answer to ecology;
a hypothetical dirty facility emits +2 through the identical stat. There is no second stat for
the positive case.

**Why this one is not a `StatId_t`.** Continuous effects are *queried* — pooled from live
emitters and re-read on every resolve — so a contribution lasts exactly as long as the thing
emitting it. A tectonic detonation has no emitter the instant it happens: `DestroyUnit` is the
next entry in its own `on_detonate_effects`, so by the time anything resolves eco damage the
missile is gone. There is nothing left to pool. The damage has to be **recorded**, not emitted,
and recording is what the triggered family is for.

That gives the plan a clean dividing line, and the clean-mineral grant is already on the same
side of it for the same structural reason — the cap grant must outlive the Tree Farm being
scrapped:

| The contribution… | Channel |
|---|---|
| should track its emitter, and vanish with it | a `StatId_t` — `EcoMineralOffset`, `EcoDamageReduction`, `EcoTerraformScale` |
| must outlive whatever caused it | an `EcologyLedger` tally, written by a triggered effect — `AddVirtualMinerals`, `GrantCleanMinerals` (and blooms, written by the stage) |

A facility that is dirty *while it stands* is the first row, and `EcoMineralOffset` already
serves it. A missile that wrecked something and then ceased to exist is the second.

No triggered effect carries a `StatId_t`, and that is deliberate rather than an omission —
`TriggeredEffect.h` says keeping the two families apart "is what makes *a StatModifier that
fires once* and *a Rebel that applies continuously* unrepresentable instead of silently doing
nothing". Every triggered effect names the specific quantity it mutates (energy, XP, population,
hit points), and uses `ModifierOp_t` only for *how* to apply, never for *which stat*.

The supported seam between the families is **`amount_source`**: a continuous effect whose
amount is read from live subject state, the way `BasesOwned` and `BaseSize` already work. So a
triggered effect reaches a stat by mutating state an `amount_source` reads. That would let the
ledger's virtual minerals feed `EcoMineralOffset` directly and collapse the `virtual_minerals`
input into it.

**Not taken, for one concrete reason:** it only pays off if *both* contributors go that way, and
the atrocity half cannot cheaply. Weighting records needs `AtrocitiesConfig_t` inside the
amount-source evaluation, which means stamping another config pointer onto `EffectContext_t`
(as `pTileYieldRules` is stamped for `ElevationEnergy`). Routing only the ecology half through a
stat while atrocities stay a direct call leaves two inputs anyway, so it buys nothing. Both stay
direct reads, summed into one `virtual_minerals` input.

> **Trap:** author `AddVirtualMinerals` **before** `DestroyUnit` in the list, as below.
> `DestroyUnit` is conventionally last for exactly this reason — `ExplosionEffect_t` spares the
> subject unit so "a following `DestroyUnit` can spend it".

**The triggered effect:**

```json
{ "type": "AddVirtualMinerals", "parameters": { "amount": 5 } }
```

It is a per-faction-subject arm like `RecordMindControl`. `ApplyDetonation` builds its context
from the unit's faction, so it credits the detonating faction. The tally is permanent, because
the atrocity ledger sets that precedent ("a record is never removed, so the eco term stays
stable for the rest of the game") and a half-decaying term would be a second rule nothing
sources. It carries no Charter gate: this channel is for physical consequences, and the Charter
is a legal instrument.

Unlike mind control, where the weight is on the reader (`mind_control_divisor`), this weight
sits on the **writer**. That is deliberate: every contributor has its own size (a tectonic
strike, nerve gas), and a reader-side weight could only apply one number to all of them. The
atrocity half already works this way — `eco_virtual_minerals` is authored per severity in
`config/atrocities.json`.

That split is what finally lets the Tectonic Payload be modelled honestly. It is **not** an
atrocity — it writes no record, takes no integrity hit, triggers no vendetta — but its
detonation still wrecks the ecology, so it authors the physical half alone, in
`config/unit_components/specials.json` beside the quake it already causes:

```json
"on_detonate_effects": [
  { "type": "Earthquake",         "parameters": { "levels_stat": "earthquake_levels" } },
  { "type": "AddVirtualMinerals", "parameters": { "amount": 5 } },
  { "type": "DestroyUnit" }
]
```

The magnitude is **not sourced** — see [Rules decisions](#rules-decisions-needed). Authoring it
explicitly is the point: it is one number in a config file, and deleting the entry turns the
rule off without touching code.

### What stays out of the effects system

Three things in the formula are deliberately **not** stats, because nothing in the game would
ever emit them and a stat with no emitters is resolve cost plus config surface for nothing:

| Constant | Why it stays in `eco_damage.lua` |
|---|---|
| `/ 8` terraform divisor | a shape constant of the sum, not a rate anything tunes |
| `/ 300` | same — it is what converts the damage factor into a percentage |
| the `1 +` in `1 + damage_reduction` | structural: it makes zero good facilities the identity, not a divide-by-zero |

`calculator-config.mdc` already blesses the Lua file as a home for a calculator's coefficients,
so these are compliant where they are. The bar for a new `StatId_t` is *something wants to
modify this at runtime* — per effects-system.md, split a stat when the **quantity** differs, not
merely to route a number through the system.

`EcoMineralOffset` clears that bar and the three above do not, which is the whole distinction:
it exists because the orbital-minerals rule was otherwise an unsolved attribution problem.
`GetMineralProduction` returns one number and nothing in it says which minerals fell from orbit.
The alternatives were a C++ scan for `orbital: true` buildings (eco learning what "orbital"
means) or re-resolving minerals twice with a filtered pool. Instead the building that adds the
minerals also declares that they do not answer to ecology, one line beside the other, and any
future "these minerals are clean" source is the same one line.

### The multiplier is one stack

`Difficulty × Perihelion × Life × max(1, 3 − PLANET)` is not four inputs. It is one number —
the resolved `EcologicalDamage` stat — and every term is an authored `MultiplyGeometric`
contribution to it, the way `difficulty.json` already contributes 3 / 5. The calculator resolves
it once with seed `1.0` and the formula multiplies by it once.

| Term | Emitter | Entry |
|---|---|---|
| Difficulty | `config/difficulty.json` *(already shipping)* | `FactionGlobal MultiplyGeometric` 3 on citizen…librarian, 5 on thinker / transcend |
| PLANET | `config/social_rating_effects.json`, the `planet` level table | one `MultiplyGeometric` per level: −3 → 6, −2 → 5, −1 → 4, **0 → 3**, +1 → 2, +2 → 1, +3 → 1 |
| Native life | `config/native_life_levels.json` | `FactionGlobal MultiplyGeometric` 1 / 2 / 3 for rare / normal / abundant |
| Perihelion | `config/world_events.json` | `WorldGlobal MultiplyGeometric 2`, collected only while the event is active |

**`EcologicalDamage` should be reclassified `PureMultiplier`.** It is declared `RawScaled`,
whose contract is "the seed is the raw value the resolve site holds" — but every use here
resolves it with seed `1.0` purely to harvest a product, which is `PureMultiplier` semantics
wearing a `RawScaled` label. `PureMultiplier` seeds `1.0` on its own, which is exactly right,
and the stat genuinely is a pure product of difficulty × planet × life × perihelion with no raw
value underneath.

The change is one arm in `KindFor`, one `static_assert` in `ValidationTests.cpp`, and the row in
`difficulty-system.md`. `DifficultyTests`' `ResolveBaseStat(..., EcologicalDamage, 1.0) == 3.0`
passes either way, because it passes the seed explicitly.

> **Trap if it stays `RawScaled`:** a `MaxClamp` authored on `ecological_damage` — the obvious
> way to express "cap eco damage at 100%" — clamps the **multiplier** at 100, not the score,
> because the score is never what this stat resolves over. That is why `max_chance_percent`
> is a plain key in `eco_damage.json` and not a clamp effect.

**`max(1, 3 − PLANET)` disappears into the data.** The `planet` table is configured over
−3…+3 and `ClampSocialRatingTotal` already applies the SMAC rule that totals outside the table
use the nearest extreme's effects — so a faction at PLANET +4 clamps to the +3 row and gets ×1.
The floor is the authored value, not arithmetic in the formula.

> **Trap:** the `planet` levels in `social_rating_effects.json` are currently **empty arrays and
> have no `"0"` key at all**. `FindSocialRatingLevelEffects` returns nullptr for an absent level,
> so without a `"0"` row a faction at neutral PLANET contributes ×1 instead of ×3 and every eco
> score comes out a third of what it should be. The 0 row is load-bearing here, unlike the other
> axes where absent-0 correctly means "no effect". `ResolveSocialRatingLevelEffects` expands an
> axis's level-0 row even when no modifier touches that axis, so once the row exists it reaches
> every base.

**Native life follows Difficulty exactly**, because it is the same kind of thing: a campaign
property, not a player preference, and one a save must carry. `GameRulesConfig_t` gains
`nativeLifeLevelId` beside `difficultyId`; `config/native_life_levels.json` holds `default` plus a
`levels` array of `{ id, name, effects }`, parsed by a `NativeLifeLevelConfig_t` with the same
`FindById` / `RequireForSession` pair; and `FactionEffectsPool::CollectNativeLifeEffects_()`
mirrors `CollectDifficultyEffects_()`, appending with `sourceId` `"native_life_level"`.

> **Name collision:** `NativeLifeConfig_t` is **taken** — `game/units/NativeUnitConfig.h` uses it
> for the bloom's lifeform-spawn range in `config/native_units.json`. That is a different
> quantity (how many worms a bloom releases, not how abundant life is planet-wide), so this one
> is `NativeLifeLevelConfig_t` in `config/native_life_levels.json`. The existing
game-rules revision already invalidates every faction pool when the rules change, so switching
it mid-campaign re-resolves with no extra plumbing. World generation can later read the same id
for fungus and worm density without a second setting.

**Perihelion is a world event**, so it lives in a new `config/world_events.json` and is driven by
the `WorldEvents` stage — not a block in `eco_damage.json`. Eco-damage is a *consumer* of it; the
event itself belongs to the world. It is **not random**: it runs the sourced 20-years-in-every-80
cycle, declared as a `Cycle` trigger the stage evaluates against the mission year.

**It is collected, not conditioned.** A `Condition_t` on the entry would be the wrong tool:
`FilterBaseLevelByStatId` excludes conditional effects from base-level resolution, so a
conditional perihelion multiplier would never fire. Instead the effect is *present or absent*.
`GameState::CollectWorldExtras` — already the hook by which `WorldGlobal` effects and council
extras enter a faction's composed pool — appends the `effects` of every **active** world event.
`GetWorldCompositionStamp` folds in the active-event revision, so an event starting or expiring
invalidates every faction's pool on the turn it happens.

`CollectWorldExtras` does not care *why* an event is active, which is what keeps this general:
the same path serves a timed random event, a scripted one, and the sea-level rise the fungal-pop
outcome will eventually want.

### Clean minerals over time

`Cleanmins = 16 + blooms + eco facilities built since the first bloom`. The 16 is a config
effect; the other two are `EcologyLedger` tallies, because SMAC's grant is **permanent** — it survives the
facility being sold or destroyed — and so cannot be a continuous effect. The sum happens in
`eco_damage.lua`, not C++: the engine hands the formula `clean_minerals`, `fungal_blooms` and
`clean_mineral_grants` separately, so a mod that wants blooms to count double changes the formula
rather than the engine.

A new triggered effect carries the grant, placed in `on_complete_effects` on Tree Farm, Hybrid
Forest, Centauri Preserve and Temple of Planet — and **not** on Nanoreplicator, which raises
`Goodfacs` but never the cap:

```json
{ "type": "GrantCleanMinerals", "parameters": { "amount": 1, "requires_first_bloom": true } }
```

`requires_first_bloom` is the "since the first bloom" quirk that Apolyton #175, *Ecology
(Revised)* and the CivFanatics thread all describe: with it set, the arm reads
`EcologyLedger::FungalBlooms` and credits nothing before the faction's first bloom. Both
parameters are required. The gate lives on the entry rather than in `eco_damage.json` because
the dispatcher has no path to the eco config, and giving it one for a single flag would couple
`TriggeredEffectDispatch` to a subsystem.

> **Trap:** the flag sits on four separate entries, one per facility. Setting it wrong on one of
> them is silent: that facility credits the cap from turn one. `CleanMineralsTests` checks all
> four buildings rather than one representative.

**"Built, not acquired" is why the grant lives in `on_complete_effects`.** The sources are
specific that the facility must be *built*: a captured or granted one does not raise the cap.
That falls out of the slot rather than needing a check, because `on_complete_effects` fires from
exactly one place — `ApplyProductionCompleteEffects_`, on a `ProductionCompleted_t`. The
triggered `AddBuilding` effect and the direct `BuildingManager::AddBuilding` calls behind base
capture and the free Headquarters never reach it, so a facility that arrives any way other than
being produced credits nothing.

> **Trap:** this makes the slot choice a rule, not a detail. Moving the grant to a continuous
> `FactionGlobal` effect would break two rules at once — it would credit acquired facilities and
> stop being permanent when one is scrapped — and both failures are silent.

### The ecology ledger

`EcologyLedger` (`include/game/ecology/EcologyLedger.h`) is a sibling of `AtrocityLedger` and
`MindControlLedger`: `GameState` owns one, and it holds three per-faction tallies.

```cpp
class EcologyLedger
{
public:
    void RecordFungalBloom(FactionId_t faction);
    void GrantCleanMinerals(FactionId_t faction, int amount);  // amount must be positive
    void AddVirtualMinerals(FactionId_t faction, int amount);  // amount must be positive

    int FungalBlooms(FactionId_t faction) const;        // 0 when it has none
    int CleanMineralGrants(FactionId_t faction) const;
    int VirtualMinerals(FactionId_t faction) const;

    Revision GetRevision() const;                       // bumped by every write

private:
    std::map<FactionId_t, int> m_fungalBlooms;
    std::map<FactionId_t, int> m_cleanMineralGrants;
    std::map<FactionId_t, int> m_virtualMinerals;
    Revision m_revision;
};
```

It is one ledger for the ecology concern, not three, because all three tallies feed the same
score and the same memo. A single revision covers them.

The revision is the one thing `MindControlLedger` lacks. The probe cost quote is not memoized,
so mind control never needed one, but the eco score is, and a bloom, a grant or a tectonic
strike moves it without touching any effect pool.

| Writer | Tally |
|---|---|
| `EcoDamage` stage, directly | `RecordFungalBloom` |
| `GrantCleanMinerals` triggered effect | `GrantCleanMinerals` |
| `AddVirtualMinerals` triggered effect | `AddVirtualMinerals` |

Both triggered effects are per-faction-subject arms, like `RecordMindControl`. Each pushes a
result (`CleanMineralsGranted_t`, `VirtualMineralsAdded_t`), and each parses `amount` with
`is_number_integer()` and rejects values below 1, as `ParseRecordMindControl_` does.
`GrantCleanMinerals` returns `false` when the first-bloom gate withholds it, so a `oncePer` key
on it stays unspent.

The ledger is save data and serializes with `GameState`, alongside the other two, once that is
wired.

## Config shape

### `config/eco_damage.json`

```json
{
  "effects": [
    {
      "type": "StatModifier",
      "scope": "FactionGlobal",
      "parameters": { "stat": "eco_clean_minerals", "amount": 16, "op": "Add" }
    }
  ],
  "fungal_pop": {
    "max_chance_percent": 100,
    "on_pop_effects": [
      { "type": "FungalBloom", "parameters": { "tiles": 1 } }
    ]
  }
}
```

Every key is required at load; there is no C++ default standing in for a missing one.

### `config/world_events.json`

A registry of world events, new with this work. Perihelion is its first entry; it is not an
eco-damage file and eco-damage does not parse it.

```json
{
  "events": [
    {
      "id": "Perihelion",
      "name": "Solar Perihelion",
      "trigger": {
        "kind": "Cycle",
        "cycle_years": 80,
        "duration_years": 20,
        "start_year_offset": 0
      },
      "effects": [
        {
          "type": "StatModifier",
          "scope": "WorldGlobal",
          "parameters": { "stat": "ecological_damage", "amount": 2, "op": "MultiplyGeometric" }
        }
      ],
      "on_start_effects": [],
      "on_end_effects": []
    }
  ]
}
```

- `effects` are continuous and apply **while the event is active** — served to every faction
  through `CollectWorldExtras`. `on_start_effects` / `on_end_effects` are triggered slots for
  one-shot consequences; both ship empty.
- A `Cycle` trigger is deterministic and needs no stored state: the event is active whenever
  `(missionYear − start_year_offset) mod cycle_years < duration_years`. `WorldEvents` compares
  that predicate against the previous turn's value to fire `on_start_effects` / `on_end_effects`
  on the edges.
- `trigger.kind` exists because this is a *registry*, and the stage it feeds is named for random
  events — but `Cycle` is the only kind this work implements. The difficulty rule
  `random_events_after_turn` (parsed into `DifficultyRules_t::randomEventsAfterTurn`, still read
  by nothing) gates random events specifically, so a cyclic Perihelion does **not** consume it
  and its `TODO(difficulty)` in `WorldEvents::ExecuteImpl` stays open.

### `config/eco_damage.lua`

Mirrors `tech_cost.lua`: a documented variable list, one function, and a returned table naming
the formula.

```lua
-- Variables set by the engine before evaluating damage_formula:
--   terraform_raw, terraform_scale, minerals, mineral_offset, clean_minerals,
--   fungal_blooms, clean_mineral_grants, virtual_minerals, damage_reduction, techs,
--   eco_scale
--
-- clean_minerals is the resolved eco_clean_minerals stat (the base 16); fungal_blooms and
-- clean_mineral_grants are the faction's EcologyLedger tallies, raw.
--
-- virtual_minerals arrives pre-weighted. Do not multiply by 5 here: that factor is
-- Major.eco_virtual_minerals in config/atrocities.json, and every AddVirtualMinerals
-- entry likewise authors its own amount.
--
-- eco_scale is the resolved EcologicalDamage stat: difficulty x Planet rating x native
-- life x perihelion, already multiplied together by the effect stack.

function eco_damage_formula()
    local terraform = (terraform_raw / 8) * terraform_scale

    local cap = clean_minerals + fungal_blooms + clean_mineral_grants

    local clean1 = 0
    if terraform > 0 then clean1 = math.min(cap, terraform) end
    local clean2 = cap - clean1

    local mineral_term = (minerals + mineral_offset - clean2 + virtual_minerals) / (1 + damage_reduction)
    local factor = math.max(0, math.floor((terraform - clean1) + mineral_term))

    return math.floor(factor * techs * eco_scale / 300)
end

return { damage_formula = "eco_damage_formula()" }
```

### `config/improvements.json`

Each counting improvement gains its two `ThisTile` entries, e.g. Thermal Borehole:

```json
{ "type": "StatModifier", "scope": "ThisTile",
  "parameters": { "stat": "eco_damage_contribution", "amount": 9, "op": "Add" } },
{ "type": "StatModifier", "scope": "ThisTile",
  "parameters": { "stat": "eco_damage_worked_contribution", "amount": 9, "op": "Add" } }
```

and the sea-base term rides the existing `Base` improvement:

```json
{ "type": "StatModifier", "scope": "ThisTile",
  "condition": { "kind": "TargetTileHas", "value": "Water" },
  "parameters": { "stat": "eco_damage_contribution", "amount": 1, "op": "Add" } }
```

### `config/buildings/buildings.json`

Tree Farm, Hybrid Forest, Centauri Preserve and Temple of Planet each carry the gated
`GrantCleanMinerals` in `on_complete_effects`, shown under
[Clean minerals over time](#clean-minerals-over-time). They land when those buildings are
authored (see [Missing systems](#missing-systems)).

## Code additions

| File | Change |
|---|---|
| `include/game/effects/EffectEnums.h` | six `StatId_t` enumerators, `ParseStatId` entries, `KindFor` / `DomainFor` arms, `EffectSourceKind_t::EcoDamage` |
| `include/game/ecology/EcologyLedger.h` + `src/game/ecology/EcologyLedger.cpp` | the ledger and its revision |
| `include/game/GameState.h` + `.cpp` | own `m_pEcology`; `GetEcologyLedger()` const and non-const, beside `GetMindControlLedger()` |
| `include/game/effects/TriggeredEffect.h` | `GrantCleanMineralsEffect_t { int amount; bool bRequiresFirstBloom; }` and `AddVirtualMineralsEffect_t { int amount; }` in `TriggeredEffectVariant_t` |
| `include/game/effects/TriggeredEffectDispatch.h` | `CleanMineralsGranted_t`, `VirtualMineralsAdded_t` in `TriggeredEffectResult_t` |
| `src/game/effects/TriggeredEffectParser.cpp` | `ParseGrantCleanMinerals_` / `ParseAddVirtualMinerals_` + type-table entries |
| `src/game/effects/TriggeredEffectDispatch.cpp` | both in `IsPerFactionSubject_`, and an `ApplyOne_` arm for each writing the ledger |
| `src/game/EffectReferenceValidator.cpp` | empty visitor arms for both |
| `include/game/ecology/EcoDamageConfig.h` + `src/game/ecology/EcoDamageConfigParser.cpp` | `EcoDamageConfig_t` and `EcoDamageConfigParser`; required keys, `ParseEffects(..., EffectSourceKind_t::EcoDamage, ...)`, and the formula `eco_damage.lua` returns |
| `include/game/ecology/EcoDamageCalculator.h` + `.cpp` | `EcoDamageInputs_t`, Lua bridge, negative rejection (`EvalInt` already rejects non-finite) |
| `include/game/ecology/BaseEcology.h` + `.cpp` | input assembly, the terraform walk, and the memo |
| `include/game/GameDataPaths.h` | `ecoDamage = "config/eco_damage.json"`, `ecoDamageFormula = "config/eco_damage.lua"` |
| `src/game/GameDataContext.cpp` | own `ecoDamageConfig`, `nativeLifeLevelConfig`, `worldEventsConfig` and `ecoDamageCalculator`; the Lua runtime is created before `eco_damage.lua` loads, ahead of `ValidateEffectReferences` |
| `src/game/EffectReferenceValidator.cpp` | walk `eco_damage.json`'s `effects` and `fungal_pop.on_pop_effects`, every native life level, every world event's lists, and probe `on_paid_effects` |
| `include/game/faction/base/BaseManager.h` + `.cpp` | owns `BaseEcology`; `GetEcologicalDamage()` |
| `config/unit_components/specials.json` | `Tectonic_Payload` gains `AddVirtualMinerals 5` in its `on_detonate_effects`, before `DestroyUnit` |
| `include/game/stages/EcoDamage.h` + `src/game/stages/EcoDamage.cpp` | the per-faction stage, `TurnStageRegistrar<EcoDamage>` |
| `config/turn_stages.json` | the `EcoDamage` entry between `WorldEvents` and `VictoryConditionChecks` |
| `config/buildings/buildings.json` | `Nessus_Mining_Station` gains `AllOwnerBases eco_mineral_offset -1` beside its `minerals +1`; the four eco facilities, once authored, carry `GrantCleanMinerals` |
| `config/social_rating_effects.json` | fill the `planet` level table, **including a new `"0"` row** |
| `config/native_life_levels.json` + `include/game/NativeLifeLevelConfig.h` + parser | `default` and a `levels` array of `{ id, name, effects }`; `FindById` / `RequireForSession` mirroring `DifficultyConfig_t`. **Not** `NativeLifeConfig_t`, which is taken |
| `include/game/GameRulesConfig.h` | `nativeLifeLevelId` beside `difficultyId` |
| `src/game/GameSettings.cpp`, `src/game/Engine.cpp` | `game_rules.native_life` load/save; `Engine` validates the id like difficulty and calls `CreateWorldEvents` |
| `src/game/faction/FactionEffectsPool.cpp` | `CollectNativeLifeEffects_()` mirroring `CollectDifficultyEffects_()`, and `CollectEcoDamageEffects_()`, appended in `Rebuild_` |
| `config/world_events.json` + `include/game/world-events/WorldEventConfig.h` + `.cpp` | the event registry and `WorldEventsConfigParser`; `IsWorldEventActive`; `EffectSourceKind_t::WorldEvent`; `GameDataPaths::worldEvents` |
| `include/game/world-events/WorldEventTracker.h` + `.cpp` | the active set and its `Revision`; `Advance` returns the events that started and ended |
| `include/game/GameState.h` + `.cpp` | `CreateWorldEvents` / `GetWorldEvents`; `CollectWorldExtras` and `CollectSessionWorldEffects` append every active event's `effects`; `GetWorldCompositionStamp` folds in the tracker revision |
| `src/game/stages/WorldEvents.cpp` | advance the tracker against the years since the first playable year and fire `on_start_effects` / `on_end_effects` on the edges, every faction as subject |
| `include/lib/GameEvent.h` | `EvFungalBloom` |
| `include/game/PauseOnEventsConfig.h`, `GameSettings.cpp`, `SettingsPanel.cpp` | the `FungalBloom` pause event and its `fungal_bloom` toggle |

## Rules decisions needed

Two questions that were open are now sourced, and belong in `docs/game-rules-decisions.md` as
answers rather than assumptions — both from *Ecology (Advanced)*, quoted under
[The rules](#the-rules-interpreted):

1. **The counted set is Mines, Solar Collectors, Farms, Soil Enrichers, Roads, Mag Tubes,
   Condensers, Mirrors, Boreholes.** Sensors, Bunkers and Airbases do not count.
2. **The sum counts improvements, not squares** — a tile holding a Road and a Mine contributes
   2, or 4 when worked.

Still open:

3. **Whether the fungal-pop roll can fire more than one pop per faction per turn.** The stage
   as designed rolls every base independently.
4. **Which year the Perihelion cycle counts from.** The sources give the shape — 20 years in
   every 80 — but never the phase. `world_events.json` carries `start_year_offset`, assumed 0
   (the first playable year); a different epoch is a one-key change.
5. **How much eco damage a tectonic detonation causes.** That it is **not an atrocity** is
   settled: the Datalinks never call it one, and it may not target bases or units, so it cannot
   commit one — it writes no record, takes no integrity hit, triggers no vendetta. What is *not*
   settled is the ecological half. *Ecology (Revised)* puts `TectonicPayloadsUsed` in the
   atrocity term but tags it `[confirm]`, and no other source mentions it. The plan authors
   `AddVirtualMinerals 5` — the weight that source implies — as one tunable number. The
   effect rejects an amount of 0, so turning the rule off means deleting the entry. Whether it should be ungated (as authored) or follow the Charter like an atrocity is the
   same open question.
6. **Which tile in the base radius the pop lands on.** The sources only say a pop happens
   "within the base radius". The stage picks uniformly among the base's workable tiles that are
   not bases and do not already have fungus; weighting toward the tile that contributed most
   eco-damage (the borehole that caused it) would read better but is not sourced. `FungalBloom`
   itself then handles spread from that origin.
7. **Whether sea terraforming counts.** *Ecology (Advanced)*'s list is land-only, and neither it
   nor the others say what Mining Platforms or Tidal Harnesses contribute. They are authored at
   0 until ruled on; Kelp Farms are the one sea improvement the sources do place (counted, but
   never doubled for being worked).

## Missing systems

Things the eco score or its outcome wants that this repo does not have yet. Each is a stub
with a named input, not a silent zero.

| Gap | Effect on this plan |
|---|---|
| Tree Farm, Hybrid Forest, Centauri Preserve, Temple of Planet, Nanoreplicator are not in `config/buildings/buildings.json` | `EcoTerraformScale` and `EcoDamageReduction` have no emitters until they are authored; the stats resolve to their identity seeds meanwhile |
| Pholus Mutagen and Singularity Inductor are not in `projects.json` | same, for the faction-wide half of `Goodfacs` |
| ~~No atrocity ledger~~ — **built**. `AtrocityLedger::EcoVirtualMinerals(perpetrator, rConfig)` sums `eco_virtual_minerals` over that faction's **counted** records; see `docs/architecture/atrocity-system.md` | add that call to `EcologyLedger::VirtualMinerals` to form `virtualMinerals`, and keep the `5 *` out of the formula — the factor is `Major.eco_virtual_minerals` in `config/atrocities.json`, `Simple` is 0. Gated by `bCounted`, so an act committed while the Charter was repealed never reaches the eco term. Takes `AtrocitiesConfig_t`, owned by `GameDataContext::atrocitiesConfig`. Tectonic payloads add nothing **to the ledger**; their eco damage goes through `EcologyLedger` — see [Rules decisions](#rules-decisions-needed) |
| Orbital minerals are **live** (`Nessus_Mining_Station` emits `AllOwnerBases minerals +1`) but not subtracted | not a deferred gap — a day-one correctness bug. SMAC excludes orbital minerals from the eco term, so shipping without the subtraction charges eco damage for minerals nobody terraformed for. Solved here by `EcoMineralOffset`, authored beside the `minerals +1` — see [What stays out of the effects system](#what-stays-out-of-the-effects-system) |
| No native life **abundance** setting | added here as `GameRulesConfig_t::nativeLifeLevelId` + `config/native_life_levels.json`, the Difficulty shape; needs a new-game menu row |
| No world-event system | `config/world_events.json` and the active-event set are built here, minimally: `WorldEvents` currently only spreads terraform improvements. Perihelion is the only shipping entry, and `Cycle` the only trigger kind |
| ~~No mind worm or native unit spawning~~ — **built**. `ApplyFungalBloom` plants the fungus, lets the tile drop incompatible improvements, and spawns native lifeforms from `NativeUnitRegistry` onto the new tiles | nothing left to do: the whole outcome is one authored `FungalBloom` entry in `on_pop_effects`. Requires a native-life faction in the session — `ApplyFungalBloom` throws without one when a lifeform would spawn |
| No sea level / global warming | the consequence of sustained global eco-damage. The config hook exists — the `melt_polar_caps` council proposal already emits `WorldParameter sea_level +1` — but `WorldParameter` is an unimplemented arm in `TriggeredEffectDispatch`, so nothing happens yet |
| No base-screen eco row | the number is computed and reachable but nothing renders it; `BaseView` follow-up |

## Tests

- `ParserTests.cpp` — the six new stat wire forms round-trip through `ParseStatId`;
  `GrantCleanMinerals` and `AddVirtualMinerals` parse, and are rejected with a missing, zero,
  negative or fractional `amount`; `GrantCleanMinerals` without `requires_first_bloom` is
  rejected; both fail with the one-shot message in a continuous `effects` list.
- `TriggeredEffectTests.cpp`:
  - `GrantCleanMinerals` with the gate set credits nothing at 0 blooms, returns false and leaves
    a `oncePer` key unspent; it credits at 1 bloom;
  - with the gate cleared it credits at 0 blooms;
  - both arms credit each faction in a multi-faction context once, and credit the actor, not
    the base's owner, in a probe mission context;
  - every ledger write bumps `EcologyLedger::GetRevision()`.
- `ValidationTests.cpp` — `static_assert` on `KindFor` / `DomainFor` for each.
- New `tests/game/EcoDamageTests.cpp` (score inputs through `BaseEcology`, the fixture formula,
  and the config parser):
  - a base under its cap scores 0;
  - one borehole worked contributes `18`, idle `9`, and a kelp farm contributes `1` either way;
  - a borehole on a tile held by a **supply crawler** contributes `9`, not `18`, and still `9`
    when the crawler is homed at a different base;
  - a worked tile with a Road and a Mine contributes `4`, and `6` once a Mag Tube joins them —
    improvements stack, squares do not collapse;
  - a Sensor, a Bunker and an Airbase contribute `0` worked or idle;
  - Tree Farm halves and Hybrid Forest zeroes the terraform term;
  - `EcoDamageReduction` divides only the mineral term, not the terraform term;
  - a faction-wide cap applies in full at each of two bases;
  - the documented worked example reproduces on Librarian and doubles-and-some on Transcend
    (difficulty 3 → 5).
- `EcoMineralOffset` — a base with two Nessus Mining Stations scores as though it produced two
  fewer minerals, while its actual mineral output is unchanged; a base with none is unaffected.
- Virtual minerals, all three channels into one input:
  - a counted Major atrocity adds 5, a Simple act adds 0, and an act committed with the Charter
    repealed adds 0; committing one mid-turn invalidates the eco memo rather than serving a
    stale score;
  - `AddVirtualMinerals` adds its authored amount with the Charter in force **and** repealed —
    this channel is ungated — and writes no atrocity record; in `ExplosionTests.cpp`, a fixture
    warhead's charge lands before its `DestroyUnit` spends it;
  - the contribution is divided by `(1 + Goodfacs)`, so a Centauri Preserve halves a tectonic
    scar exactly as it halves a foundry — the guard against it being bolted onto the score;
  - a positive `EcoMineralOffset` raises the score by the same amount a negative one lowers it.
- New `tests/game/EcoScaleTests.cpp` — the multiplier stack:
  - a faction at PLANET 0 resolves `EcologicalDamage` to `3 × difficulty`, which is the
    regression guard on the `"0"` row existing;
  - PLANET +2 and +3 both resolve to ×1, and PLANET +4 clamps to the +3 row;
  - rare / normal / abundant native life scale the same base 1× / 2× / 3×;
  - an active Perihelion event doubles a base's resolved `EcologicalDamage`, and ending it
    restores the prior value rather than serving a stale pool.
- New `tests/game/NativeLifeLevelConfigTests.cpp` — the native life level parser requires
  every key and resolves the session level.
- Clean-mineral grants in `tests/game/EcoDamageStageTests.cpp`, against fixture eco facilities:
  - **each of the four** eco facilities completed before the first bloom credits nothing;
  - after the first bloom each credits 1, and the grant survives the facility being scrapped;
  - a Tree Farm that arrives by capture or by a triggered `AddBuilding` credits nothing at any
    time;
  - a Nanoreplicator credits nothing while still counting toward `EcoDamageReduction`;
  - a grant credited mid-turn invalidates the eco memo. That is the guard on the ledger
    revision.
- Turn-stage test — a base with a 100% score blooms, `EcologyLedger::FungalBlooms` increments
  for its owner, and `EvFungalBloom` fires; a 0% score does neither.
- Outcome wiring, against the real `FungalBloom` effect rather than a stub:
  - the pop tile is inside the base radius, is never the base tile, and is never a tile that
    already had fungus;
  - after a pop the tile carries fungus, its incompatible improvements are gone, and the
    session's native-life faction owns at least `fungal_bloom_native_lifeforms_min` new units;
  - **a context missing `pTile` fires nothing** — the regression guard on the trap above, since
    `FungalBloom_` returns false rather than throwing;
  - the second bloom raises `Cleanmins` by 2 in total, so blooms compound the cap.
- New `tests/game/WorldEventTests.cpp` — the parser; Perihelion is active for mission years
  0–19, inactive for 20–79, active again at 80; `on_start_effects` / `on_end_effects` fire once on each edge and
  not on the turns between; and its `effects` reach every faction's pool while active and no
  faction's once it ends.
- Stage-ordering test — a Perihelion that begins this turn is already in the stack when
  `EcoDamage` rolls in the same turn.
- The cap test reads `eco_damage.json`'s `FactionGlobal` baseline at two bases, which is the
  proof an `EcoDamage`-sourced effect reaches the faction lane.
- `GameSettingsTests.cpp` — `native_life` and the `fungal_bloom` pause toggle round-trip.
- Fixtures: `tests/fixtures/eco_damage.json`, `eco_damage.lua`, `native_life_levels.json`,
  `native_units.json` and `world_events.json`; eco contributions on `tests/fixtures/improvements.json`; a `planet` axis in
  `social_rating_effects.json`; eco facilities and a stackable PLANET building in
  `buildings.json`.

Run with `./bd test`.

## Docs to update

- New `docs/architecture/ecology-system.md` — the pipeline diagram and the stat table.
- `docs/architecture/high-level.md` — the new subsystem and its stage.
- `docs/architecture/turn-system.md` and `docs/game-rules/turn-structure.md` — the `EcoDamage`
  stage and its ordering constraint.
- `docs/architecture/effects-system.md` — the six stats in the StatId list, `EcoDamage` in the
  source-kind list, the `eco_damage.json` row in the trigger-slot table, `GrantCleanMinerals` and
  `AddVirtualMinerals` in the triggered type list and the per-faction-subject list, and a short
  section beside `RecordMindControl` naming `EcologyLedger`.
- `docs/architecture/difficulty-system.md` — the `EcologicalDamage` row is live, not pending, and
  difficulty is now one contributor to a shared stack rather than its only emitter.
- `docs/architecture/faction-system.md` — `CollectNativeLifeEffects_` beside
  `CollectDifficultyEffects_`, and active world events as a `CollectWorldExtras` contributor.
- `docs/architecture/turn-system.md` — the `WorldEvents` stage now owns a real event registry
  and the new `EcoDamage` stage follows it.
- `docs/plans/difficulty.md` — the eco rows are no longer blocked on a missing stat.
