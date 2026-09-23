---
name: Ecological damage
overview: "Per-base eco-damage score assembled from tile/base/faction effect contributions and a Lua formula in `config/eco_damage.lua`, a faction-wide clean-minerals cap raised by fungal blooms and eco facilities, and a per-faction `EcoDamage` turn stage that rolls the score as a fungal-pop percentage. The pop outcome is stubbed behind a config trigger slot."
todos:
  - id: stats
    content: Add EcoDamageContribution / EcoDamageWorkedContribution / EcoTerraformScale / EcoCleanMinerals / EcoDamageReduction to StatId_t, ParseStatId, KindFor, DomainFor
    status: pending
  - id: config
    content: Add config/eco_damage.json + config/eco_damage.lua, EcoDamageConfig_t/parser, EffectSourceKind_t::EcoDamage, GameDataPaths + LoadGameData
    status: pending
  - id: contributions
    content: Author eco contributions on improvements.json (Borehole/Mirror/Condenser/Forest/Farm/Mine/Solar/SoilEnricher/KelpFarm/Base-on-water)
    status: pending
  - id: calculator
    content: EcoDamageCalculator (Lua bridge) + EcoDamageInputs_t; BaseManager::GetEcologicalDamage() memoized on the base effects revision
    status: pending
  - id: faction-state
    content: Faction fungal-bloom counter + clean-mineral grant counter; GrantCleanMinerals triggered effect
    status: pending
  - id: stage
    content: EcoDamage per-faction turn stage between Population and WorldEvents; roll + EvFungalBloom + player notice; on_pop_effects trigger slot (stub)
    status: pending
  - id: eco-scale
    content: "Eco multiplier emitters: planet levels in social_rating_effects.json, config/native_life.json + GameRulesConfig.nativeLifeId + FactionEffectsPool"
    status: pending
  - id: world-events
    content: config/world_events.json registry with a Cycle trigger, active-event state on GameState, WorldEvents starts/expires events, CollectWorldExtras serves active effects; Perihelion is the first entry
    status: pending
  - id: docs
    content: docs/architecture/ecology-system.md; update high-level, turn-system, effects-system, difficulty-system, game-rules/turn-structure
    status: pending
  - id: tests
    content: Parser, calculator, contribution-routing, cap, stage-roll tests; ./bd test
    status: pending
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
                       + (Minerals - Cleanmins2 + 5 * Atrocities) / (1 + Goodfacs) )   -- floored at 0
EcoDamage%    = DamageFactor * Perihelion * Techs * Life * Difficulty * max(1, 3 - PLANET) / 300
```

The result is a **percentage chance of a fungal pop inside the base radius next turn** — it is
the red number on the base screen. There is no separate threshold constant: `DamageFactor`
floors at zero, so a base under its cap rolls 0%.

| Term | Meaning |
|---|---|
| tile contribution | per-improvement weight, doubled on tiles this base works |
| Tree Farm / Hybrid Forest | halve / zero the terraform term |
| Minerals | this base's mineral production after multipliers, less minerals received from orbit |
| Atrocities | major atrocities by this faction (planet busters, tectonic payloads) |
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

## Design

### 1. Score

Five new stats, one existing. Each is a distinct quantity, so each is its own `StatId_t`.

| Stat | Wire form | Kind | Domain | Emitters |
|---|---|---|---|---|
| `EcoDamageContribution` | `eco_damage_contribution` | Additive | Tile | improvements: Borehole +9, Mirror +7, Condenser +5, Farm/Mine/SolarCollector/SoilEnricher/**Road**/**MagTube** +1, KelpFarm +1, Forest −1, Base +1 when `TargetTileHas: Water`. Sensor, Bunker, Airbase, MiningPlatform, TidalHarness author nothing |
| `EcoDamageWorkedContribution` | `eco_damage_worked_contribution` | Additive | Tile | the extra weight counted only while this base works the tile: +1 on every counted improvement **except** KelpFarm and Forest — including Road and MagTube, which the counted set names |
| `EcoTerraformScale` | `eco_terraform_scale` | PureMultiplier | Base | Tree Farm `MultiplyGeometric 0.5`, Hybrid Forest `MultiplyGeometric 0` |
| `EcoCleanMinerals` | `eco_clean_minerals` | Additive | Faction | `eco_damage.json` `effects`: `FactionGlobal Add 16` |
| `EcoDamageReduction` | `eco_damage_reduction` | Additive | Base | Centauri Preserve / Temple of Planet / Nanoreplicator `ThisBase Add 1`; Pholus Mutagen / Singularity Inductor `AllOwnerBases Add 1` |
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
    int minerals = 0;              // GetMineralProduction, less orbital deliveries
    int cleanMinerals = 0;         // resolved EcoCleanMinerals + blooms + grants
    int atrocities = 0;
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

The result is memoized against the base effects revision plus the world's worked-tile and
map-appearance revisions — the same validation shape `CommerceManager` uses — because the base
screen reads it every frame. Crawl start and stop both go through `WorkedTileIndex`, so they bump
the worked-tile revision and invalidate the memo like any worker reassignment.

### 2. Threshold

There is no separate threshold check. The score **is** the percentage. A new per-faction turn
stage `EcoDamage` sits **after** `WorldEvents` and, for each of the faction's bases, rolls
`rng % 100 < min(100, score)`.

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

### 3. Outcome (stubbed)

On a successful roll the stage:

1. calls `Faction::RecordFungalBloom()`, which increments the faction's bloom counter (this is
   the `+1` to `Cleanmins` and the gate that starts crediting eco-facility grants),
2. emits `EvFungalBloom { factionId, baseId }` on the `EventBus`,
3. enqueues a `PlayerInteractionQueue` notice,
4. applies `eco_damage.json`'s `on_pop_effects` triggered list through `ApplyTriggeredEffects`
   with the base and faction stamped as subjects. **Ships empty.**

Everything a real pop does — choosing a tile in the radius, planting fungus, destroying the
improvements that do not survive, spawning natives, sea-level rise — is deliberately not written
here. `on_pop_effects` is the slot they land in once the systems they need exist; the entries
this repo cannot yet express are listed under [Missing systems](#missing-systems).

### The multiplier is one stack

`Difficulty × Perihelion × Life × max(1, 3 − PLANET)` is not four inputs. It is one number —
the resolved `EcologicalDamage` stat — and every term is an authored `MultiplyGeometric`
contribution to it, the way `difficulty.json` already contributes 3 / 5. The calculator resolves
it once with seed `1.0` and the formula multiplies by it once.

| Term | Emitter | Entry |
|---|---|---|
| Difficulty | `config/difficulty.json` *(already shipping)* | `FactionGlobal MultiplyGeometric` 3 on citizen…librarian, 5 on thinker / transcend |
| PLANET | `config/social_rating_effects.json`, the `planet` level table | one `MultiplyGeometric` per level: −3 → 6, −2 → 5, −1 → 4, **0 → 3**, +1 → 2, +2 → 1, +3 → 1 |
| Native life | `config/native_life.json` | `FactionGlobal MultiplyGeometric` 1 / 2 / 3 for rare / normal / abundant |
| Perihelion | `config/world_events.json` | `WorldGlobal MultiplyGeometric 2`, collected only while the event is active |

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
`nativeLifeId` beside `difficultyId`; `config/native_life.json` holds `default` plus a `levels`
array of `{ id, name, effects }`, parsed by a `NativeLifeConfig_t` with the same
`FindById` / `RequireForSession` pair; and `FactionEffectsPool::CollectNativeLifeEffects_()`
mirrors `CollectDifficultyEffects_()`, appending with `sourceId` `"native_life"`. The existing
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
effect; the other two are live faction state, because SMAC's grant is **permanent** — it survives
the facility being sold or destroyed — and so cannot be a continuous effect.

`Faction` gains two counters: `m_fungalBlooms` and `m_cleanMineralGrants`.

A new triggered effect carries the grant:

```json
{ "type": "GrantCleanMinerals", "parameters": { "amount": 1 } }
```

placed in `on_complete_effects` on Tree Farm, Hybrid Forest, Centauri Preserve and Temple of
Planet. `TriggeredEffectDispatch` credits `pFaction` and honours
`eco_damage.json`'s `clean_minerals.grants_require_first_bloom`, so a build before the faction's
first bloom credits nothing — the quirk all three sources describe, switchable by mods.

## Config shape

### `config/eco_damage.json`

```json
{
  "formula_file": "config/eco_damage.lua",
  "effects": [
    {
      "type": "StatModifier",
      "scope": "FactionGlobal",
      "parameters": { "stat": "eco_clean_minerals", "amount": 16, "op": "Add" }
    }
  ],
  "clean_minerals": {
    "grants_require_first_bloom": true
  },
  "fungal_pop": {
    "max_chance_percent": 100,
    "on_pop_effects": []
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
--   terraform_raw, terraform_scale, minerals, clean_minerals, atrocities,
--   damage_reduction, techs, eco_scale
--
-- eco_scale is the resolved EcologicalDamage stat: difficulty x Planet rating x native
-- life x perihelion, already multiplied together by the effect stack.

function eco_damage_formula()
    local terraform = (terraform_raw / 8) * terraform_scale

    local clean1 = 0
    if terraform > 0 then clean1 = math.min(clean_minerals, terraform) end
    local clean2 = clean_minerals - clean1

    local mineral_term = (minerals - clean2 + 5 * atrocities) / (1 + damage_reduction)
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

## Code additions

| File | Change |
|---|---|
| `include/game/effects/EffectEnums.h` | five `StatId_t` enumerators, `ParseStatId` entries, `KindFor` / `DomainFor` arms, `EffectSourceKind_t::EcoDamage` |
| `include/game/ecology/EcoDamageConfig.h` + `EcoDamageConfigParser.h/.cpp` | `EcoDamageConfig_t` and its parser; required keys, `ParseEffects(..., EffectSourceKind_t::EcoDamage, ...)` |
| `include/game/ecology/EcoDamageCalculator.h` + `.cpp` | `EcoDamageInputs_t`, Lua bridge, non-finite/negative rejection |
| `include/game/GameDataPaths.h` | `ecoDamage = "config/eco_damage.json"`, `ecoDamageFormula = "config/eco_damage.lua"` |
| `src/game/GameDataContext.cpp` | own both; load after the improvement/building registries, before `ValidateEffectReferences`; build the calculator with the other formula calculators |
| `src/game/EffectReferenceValidator.cpp` | walk the new config's `effects` and `fungal_pop.on_pop_effects` |
| `include/game/faction/base/BaseManager.h` + `.cpp` | `GetEcologicalDamage()`, its memo, and the terraform walk over `GetWorkableTiles()` |
| `include/game/Faction.h` + `.cpp` | `m_fungalBlooms`, `m_cleanMineralGrants`, `RecordFungalBloom()`, `AddCleanMineralGrant(int)`, and their getters |
| `include/game/effects/TriggeredEffect.h` | `GrantCleanMineralsEffect_t` in `TriggeredEffectVariant_t` |
| `src/game/effects/TriggeredEffectParser.cpp` | `ParseGrantCleanMinerals_` + dispatch-table entry |
| `src/game/effects/TriggeredEffectDispatch.cpp` | `ApplyOne_` arm; skip when the context has no faction |
| `include/game/stages/EcoDamage.h` + `src/game/stages/EcoDamage.cpp` | the per-faction stage, `TurnStageRegistrar<EcoDamage>` |
| `config/turn_stages.json` | the `EcoDamage` entry between `Population` and `WorldEvents` |
| `config/social_rating_effects.json` | fill the `planet` level table, **including a new `"0"` row** |
| `config/native_life.json` + `include/game/NativeLifeConfig.h` + parser | `default` and a `levels` array of `{ id, name, effects }`; `FindById` / `RequireForSession` mirroring `DifficultyConfig_t` |
| `include/game/GameRulesConfig.h` | `nativeLifeId` beside `difficultyId` |
| `src/game/faction/FactionEffectsPool.cpp` | `CollectNativeLifeEffects_()` mirroring `CollectDifficultyEffects_()`, appended in `Rebuild_` |
| `config/world_events.json` + `include/game/WorldEventConfig.h` + parser | the event registry; `EffectSourceKind_t::WorldEvent`; `GameDataPaths::worldEvents` |
| `include/game/GameState.h` + `.cpp` | active-event id set + its `Revision`; `CollectWorldExtras` appends every active event's `effects`; `GetWorldCompositionStamp` folds in the active-event revision |
| `src/game/stages/WorldEvents.cpp` | re-evaluate each event's `Cycle` predicate against the mission year, update the active set, and fire `on_start_effects` / `on_end_effects` on the edges |
| `include/game/EventTypes.h` | `EvFungalBloom` |

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
5. **Whether sea terraforming counts.** *Ecology (Advanced)*'s list is land-only, and neither it
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
| No atrocity ledger (planet busters, tectonic payloads do not exist) | `atrocities` input is 0 with a TODO at the assembly site |
| Orbital minerals are **live** (`Nessus_Mining_Station` emits `AllOwnerBases minerals +1`) but not subtracted | not a deferred gap — a day-one correctness bug. SMAC excludes orbital minerals from the eco term, so shipping without the subtraction charges eco damage for minerals nobody terraformed for. Needs a way to attribute part of a resolved `Minerals` stat to orbital sources |
| No native life session setting | added here as `GameRulesConfig_t::nativeLifeId` + `config/native_life.json`, the Difficulty shape; needs a new-game menu row |
| No world-event system | `config/world_events.json` and the active-event set are built here, minimally: `WorldEvents` currently only spreads terraform improvements. Perihelion is the only shipping entry, and `Cycle` the only trigger kind |
| No mind worm or native unit spawning | the pop cannot release natives. Planting the fungus itself is **not** a gap — `Tile::SetHasFungus` exists and `TerraformSpread` already flips it, and `Tile::RemoveImprovement` can destroy what the pop ruins |
| No sea level / global warming | the consequence of sustained global eco-damage. The config hook exists — the `melt_polar_caps` council proposal already emits `WorldParameter sea_level +1` — but `WorldParameter` is an unimplemented arm in `TriggeredEffectDispatch`, so nothing happens yet |
| No base-screen eco row | the number is computed and reachable but nothing renders it; `BaseView` follow-up |

## Tests

- `ParserTests.cpp` — the five new wire forms round-trip through `ParseStatId`.
- `ValidationTests.cpp` — `static_assert` on `KindFor` / `DomainFor` for each.
- New `tests/ecology/EcoDamageTests.cpp`:
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
- New `tests/ecology/EcoScaleTests.cpp` — the multiplier stack:
  - a faction at PLANET 0 resolves `EcologicalDamage` to `3 × difficulty`, which is the
    regression guard on the `"0"` row existing;
  - PLANET +2 and +3 both resolve to ×1, and PLANET +4 clamps to the +3 row;
  - rare / normal / abundant native life scale the same base 1× / 2× / 3×;
  - an active Perihelion event doubles a base's resolved `EcologicalDamage`, and ending it
    restores the prior value rather than serving a stale pool.
- `tests/ecology/CleanMineralsTests.cpp` — a `GrantCleanMinerals` before the first bloom credits
  nothing; after it credits 1 and survives the facility being scrapped.
- Turn-stage test — a base with a 100% score blooms, the faction's counter increments, and
  `EvFungalBloom` fires; a 0% score does neither.
- New `tests/game/WorldEventTests.cpp` — Perihelion is active for mission years 0–19, inactive
  for 20–79, active again at 80; `on_start_effects` / `on_end_effects` fire once on each edge and
  not on the turns between; and its `effects` reach every faction's pool while active and no
  faction's once it ends.
- Stage-ordering test — a Perihelion that begins this turn is already in the stack when
  `EcoDamage` rolls in the same turn.
- `UniversalRoutingTests.cpp` — an `EcoDamage`-sourced `FactionGlobal` effect reaches the faction
  lane.
- Fixtures: `tests/fixtures/eco_damage.json` and `eco_damage.lua`; eco contributions added to
  `tests/fixtures/improvements.json`.

Run with `./bd test`.

## Docs to update

- New `docs/architecture/ecology-system.md` — the pipeline diagram and the stat table.
- `docs/architecture/high-level.md` — the new subsystem and its stage.
- `docs/architecture/turn-system.md` and `docs/game-rules/turn-structure.md` — the `EcoDamage`
  stage and its ordering constraint.
- `docs/architecture/effects-system.md` — the five stats in the StatId list, `EcoDamage` in the
  source-kind list, and the `eco_damage.json` row in the trigger-slot table.
- `docs/architecture/difficulty-system.md` — the `EcologicalDamage` row is live, not pending, and
  difficulty is now one contributor to a shared stack rather than its only emitter.
- `docs/architecture/faction-system.md` — `CollectNativeLifeEffects_` beside
  `CollectDifficultyEffects_`, and active world events as a `CollectWorldExtras` contributor.
- `docs/architecture/turn-system.md` — the `WorldEvents` stage now owns a real event registry
  and the new `EcoDamage` stage follows it.
- `docs/plans/difficulty.md` — the eco rows are no longer blocked on a missing stat.
