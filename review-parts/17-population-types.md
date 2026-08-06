## Population — pop types and config

**Files:** `src/game/population/pop-types/GrowthConfigParser.cpp`, `include/game/population/pop-types/GrowthConfigParser.h`, `src/game/population/pop-types/Pop.cpp`, `include/game/population/pop-types/Pop.h`, `src/game/population/pop-types/PopCompositionConfigParser.cpp`, `include/game/population/pop-types/PopCompositionConfigParser.h`, `src/game/population/pop-types/PopTypeConfigParser.cpp`, `include/game/population/pop-types/PopTypeConfigParser.h`, `include/game/population/pop-types/PopTypeRegistry.h`

**Assessment:** This slice is small and mostly clear: `Pop` is a thin config-backed unit with tile claims and effect resolution split cleanly by scope (`ThisPop` vs `ThisBase`), and `PopTypeRegistry` enforces a single default at load. The dominant weaknesses are load-time permissiveness (growth and composition parsers accept incomplete/inert config) and role predicates that infer identity from contribution magnitudes, leaving an incomplete specialist check that will misclassify modded types.

### [M] Make `IsSpecialist` exclude talents (and any golden-age contributor)
`src/game/population/pop-types/Pop.cpp:42-45` — `IsSpecialist` is `!can_work_tile && riotContribution == 0`, which matches the header’s `!IsDrone()` wording but does **not** exclude `IsTalent()`. A non-worker with `golden_age_contribution > 0` is therefore both specialist and talent (`GetTalentCount` / `GetSpecialistCount` both count it; composition demotion of “excess talents” can convert specialists). Shipping types keep talents as workers, so this is latent — close the gap with `!IsWorker() && !IsDrone() && !IsTalent()` (or an explicit role flag in config). Related to the 3.4 role centralization, which stopped short of a closed role partition.

### [M] Inferring drone/talent identity from contribution thresholds couples role to magnitude
`src/game/population/pop-types/Pop.cpp:32-39` — `IsDrone` / `IsTalent` are `riotContribution > 0` / `goldenAgeContribution > 0`. That works for stock SMAC data, but any worker given a non-zero riot contribution becomes a composition “drone,” and any specialist given a golden-age contribution becomes a “talent” (see above). Composition already names conversion targets via `drone_type` / `talent_type`; role identity should be an explicit config field (or a closed enum) rather than a numeric threshold, so mods can grant partial contributions without changing type identity.

### [M] `GrowthConfigParser` silently defaults missing or mistyped keys
`src/game/population/pop-types/GrowthConfigParser.cpp:20-21` — Uses `json.value(..., config.*)` so omitted/`nutrients_per_po` typos load as `10` / `7` with no error, and there is no check that either value is positive (`nutrients_per_pop == 0` makes the growth threshold always 0). Contradicts the project rule against warn-and-default parsers (finding 3.8’s adopted style). Require the keys (e.g. `.at()`), validate `> 0`, and keep struct defaults only for programmatic construction.

### [M] Composition parser allows empty formulas and records unused `precedence`
`src/game/population/pop-types/PopCompositionConfigParser.cpp:27-28,43-52` — `drone_type` / `talent_type` are required, but formulas default to `""`; `LuaRuntime::EvalInt` then returns 0, so a missing formula silently means zero drones/talents forever. `precedence` is parsed into `PopCompositionConfig_t` and documented as controlling recalculation order (`PopCompositionConfigParser.h:17`) but is never read anywhere — modders who reorder it get no effect. Require non-empty formulas at parse time; either wire `precedence` or drop it until implemented (TODO elsewhere), and reject non-string precedence entries instead of skipping them.

### [M] `PopTypeRegistry::Validate_` does not check internal pop-type references
`include/game/population/pop-types/PopTypeRegistry.h:27-45` — Only enforces exactly one `is_default`. `fallback_pop_type` and `obsoletes` ids are never checked here (required tech is validated elsewhere). A typo in `fallback_pop_type` fails only at `ConvertToFallback` runtime; a bad `obsoletes` entry is silently inert in `PopTypeAvailabilityCalculator::ResolveCurrentType`. Extend `Validate_` to require every non-empty fallback/obsolete id exists in the registry (mirror `TechRegistry` prerequisite checks).

### [L] Convention and hygiene items
- `include/game/population/pop-types/PopTypeConfigParser.h:15-22` — `PopTypeConfig_t` bools/ints lack in-class initializers (unlike `GrowthConfig_t`); a default-constructed value is indeterminate until every field is assigned.
- `src/game/population/pop-types/Pop.cpp:111` vs `:130` — `ApplyTileMultipliers` rounds resolved totals; `GetSpecialistOutput` truncates via `static_cast<int>`. Align rounding once fractional specialist modifiers appear.
- `include/game/population/pop-types/GrowthConfigParser.h:17-18`, `PopCompositionConfigParser.h:23-24`, `PopTypeConfigParser.h:29-30` — Explicit empty default ctor/dtor noise; prefer `= default` only where needed or omit.
- `include/game/population/pop-types/Pop.h:41` — Comment says `!IsDrone()`; implementation uses `riotContribution == 0` (equivalent today) but neither mentions talents — keep comment in sync with the predicate fix above.

**Observed outside slice:**
- `docs/architecture/faction-system.md:266-267` — Still describes `GrowthConfigParser` loading `pop_growth.lua` / `threshold_formula`; live path is `pop_growth.json` + `nutrientsPerPop` / `maxBaseSize`.
- `config/pop_growth.lua` — Unused leftover alongside `config/pop_growth.json`.
- `src/game/GameDataContext.cpp:130-141` — Validates composition type ids exist, not that they satisfy `IsDrone` / `IsTalent`.
- `src/game/faction/base/population/PopContainer.cpp:144-151` — Local `currentDrones++` after `ConvertTo` does not re-check `IsDrone()`, so a mis-set `drone_type` can stop the loop without creating drones.
