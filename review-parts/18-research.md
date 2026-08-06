## Research — tech registry and costs

**Files:** `src/game/research/TechConfigParser.cpp`, `include/game/research/TechConfigParser.h`, `src/game/research/TechCostCalculator.cpp`, `include/game/research/TechCostCalculator.h`, `src/game/research/TechCostConfigParser.cpp`, `src/game/research/TechRegistry.cpp`, `include/game/research/TechRegistry.h`, `include/game/research/TechCostConfig.h`

**Assessment:** This is a small, readable slice: tech data is a plain `TechConfig_t`, the registry only adds prereq validation on top of `Registry`, and cost evaluation is a thin Lua bridge matching the pop-composition calculator pattern. The dominant weakness is silent acceptance of bad cost configuration — empty or failed formulas and missing `cost` fields degrade into a plausible minimum cost of 1 instead of failing at load.

### [H] Reject empty cost formulas; do not floor eval failures into cost 1
`src/game/research/TechCostConfigParser.cpp:24` — `cost_formula` defaults to `""` when absent. `src/game/research/TechCostCalculator.cpp:34-36` then evaluates that string and clamps with `std::max(1, cost)`. `LuaRuntime::EvalInt` returns `0` for an empty formula (and on formula errors — see prior finding 3.7 in `docs/code-review-findings.md`); the calculator turns that into research cost **1** with no throw. A missing or broken mod formula therefore looks like a valid cheap tech rather than a load-time failure. Require a non-empty `cost_formula` in the parser (as `PopCompositionConfigParser` requires `drone_type` / `talent_type`), and treat non-positive / failed evaluation as an error instead of silently clamping.

### [M] Prerequisite validation misses cycles
`src/game/research/TechRegistry.cpp:21-39` — self-reference and unknown prereq ids throw, but A→B→A (or longer cycles) load successfully. `ResearchManager::GetAvailableTechs` (caller) only unlocks techs whose prereqs are already discovered, so a cyclic component is permanently unreachable with no startup error. Add a cycle check in `Validate_()` (DFS / topo over the prereq graph) so bad mod trees fail at load.

### [M] Missing tech `cost` silently becomes 0
`src/game/research/TechConfigParser.cpp:26` — `techJson.value("cost", 0)` invents a zero base cost when the key is absent, against the project preference to throw on bad config. `base_cost` is already passed into the Lua vars (`TechCostCalculator.cpp:31`) and is documented as part of the formula surface (`config/tech_cost.lua`); once the formula uses it, a typo’d omission silently cheapens the tech. Require the `cost` field (allow explicit `0` if free techs are intentional).

### [M] Tech cost script load errors drop Lua diagnostics
`src/game/research/TechCostConfigParser.cpp:17-20` — on `safe_script_file` failure the throw is only `"Failed to load tech cost script '" + path + "'"`. The sibling `PopCompositionConfigParser` appends `sol::error::what()`. Mod authors get no line/message for a broken `tech_cost.lua`. Include the Lua error text in the exception.

### [L] Convention and hygiene items
- `include/game/research/TechCostCalculator.h:15` / `src/game/research/TechCostCalculator.cpp:14-16` — empty user-declared destructor; use `= default` in the header like `PopCompositionCalculator`.
- `src/game/research/TechCostConfigParser.cpp:3` — unused `#include <iostream>`.
- `include/game/research/TechRegistry.h:19` vs `src/game/research/TechRegistry.cpp:21` — header parameters are `rConfig` / `rConfigs`; definition uses `config` / `configs` / `c` (missing `r` reference prefix).
- `include/game/research/TechConfigParser.h:26-27` — empty constructor body in the `.cpp`; prefer `= default` in the header.
- `include/game/research/TechCostCalculator.h:23` — closing brace lacks `// namespace ac`.
- No dedicated tests for `TechCostCalculator` / `TechRegistry` validation (empty formula, missing cost, prereq cycles); only indirect use via research/effects fixtures.

**Observed outside slice:**
- `docs/architecture/research-system.md` — stale: documents `TechId` as `int`, a hardcoded `baseCost * (1 + 0.5 * missingPrereqs) * multiplier` formula, and registry “filter available techs” responsibility that now lives on `ResearchManager`.
- `src/lib/LuaRuntime.cpp:37-70` — `EvalInt` warns and returns `0` on formula errors (prior finding 3.7); that failure mode is what `TechCostCalculator`’s floor turns into cost 1.
