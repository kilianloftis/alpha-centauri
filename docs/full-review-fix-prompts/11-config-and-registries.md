# Package 11 — Config parsing, registries, and shared config/runtime libraries

Findings re-verified against the tree at `5277dd9`. One theme runs through all of them: a config
that is wrong should fail at load with a message naming the file, the id and the key — never
degrade into a plausible-looking number.

## Verified diagnoses

### [H] Empty/failed cost formulas become research cost 1

`TechCostConfigParser.cpp:24` defaults `cost_formula` to `""`; `LuaRuntime::EvalInt` returns `0`
for an empty formula (`LuaRuntime.cpp:40-43`) and `0` after *logging a warning to stdout* on any
error (`:57-60`, `:65-68`); `TechCostCalculator.cpp:36` then clamps with `std::max(1, cost)`.
Verified end to end: a mod with a broken `tech_cost.lua` gets a working game where every tech
costs 1.

Note `config/tech_cost.lua` already ends in `math.max(1, ...)`, so the C++ floor is not even
load-bearing for stock data — it exists only to mask failure.

**Chosen:** make each layer refuse to invent a value. The parser requires a non-empty
`cost_formula`; `EvalInt` throws; the calculator drops its floor and rejects a non-positive
result. Three independent gates, because the review's finding is that one silent default was
enough to hide the whole chain.

### [M] `LuaRuntime::EvalInt` warns-and-returns-0, and leaks variables between calls

`LuaRuntime.cpp:37-70`. Both halves verified. Variables are written as globals (`:46-49`) under a
comment claiming they are "scoped to this call" and are never removed, so a formula that omits an
input silently reads whatever the previous formula left there. `static_cast<int>` (`:63`)
truncates a non-integral result without complaint.

**Chosen:** throw on empty formula, on evaluation error (carrying Lua's message), and on a
non-integral result; clear the keys the call set once it returns, on the success and the throwing
path alike.

**Rejected:** evaluating in a fresh `sol::environment`. It is the tidier isolation, but it breaks
the stock config: `config/tech_cost.lua` defines `tech_cost_formula()` as a global function whose
`_ENV` is the main globals table, so variables set in a per-call environment would be invisible
*inside* the function — the formula would silently see `nil` for every input. Nil-ing the keys
afterwards fixes the leak without moving where the formula reads from.

### [M] `Registry::Load` destroys a good registry when the new file is invalid

`include/lib/Registry.h:29-42` — `m_configs` and `m_indexById` are replaced *before* `Validate_()`.
Verified: any subclass override that throws (`TechRegistry`, `PopTypeRegistry`,
`BuildingRegistry`) leaves the object holding the payload that was just rejected.

### [M] `ParseStringArray` accepts a wrong-typed value as an empty list

`src/lib/config/ConfigFields.cpp:23-35` — the `is_array()` test is folded into the presence test,
so `"prerequisites": "tech_x"` is indistinguishable from an absent key. Verified.

### [M] `Rational_t::ScaledInt` overflows before it can complain

`src/lib/Rational.cpp:59-73` — `num * (scale / den)` in `int`. Verified: the divisibility check
that precedes it does nothing to prevent the multiplication overflowing.

### [M] Tech prerequisite validation misses cycles

`TechRegistry.cpp:21-39` — self-reference and unknown ids throw; A→B→A does not. Verified against
`ResearchManager::GetAvailableTechs`, which only unlocks techs whose prereqs are all discovered,
so a cyclic component is permanently unreachable with no diagnostic.

### [M] Missing tech `cost` becomes 0

`TechConfigParser.cpp:26` — `techJson.value("cost", 0)`. Verified. `base_cost` is already exposed
to the formula, so a typo'd omission silently cheapens the tech the moment a formula uses it.

### [M] Tech cost script errors drop the Lua diagnostic

`TechCostConfigParser.cpp:17-20` throws without `sol::error::what()`, while its sibling
`PopCompositionConfigParser.cpp:19-22` includes it. Verified — same pattern, one of the two lost.

### [M] `GrowthConfigParser` defaults missing and mistyped keys

`GrowthConfigParser.cpp:20-21` — `json.value(...)` for both keys, no positivity check. Verified:
`nutrients_per_pop: 0` makes the growth threshold identically 0, so every base grows every turn.

### [M] Composition parser: empty formulas, and a `precedence` key nothing reads

`PopCompositionConfigParser.cpp:27-28,43-52` — `drone_type`/`talent_type` are required but the two
formulas default to `""`, which `EvalInt` turns into "zero drones and zero talents, forever".
`precedence` is parsed, documented in the header as controlling recalculation order, present in
`config/pop_composition.lua`, and read by nothing in the tree. Verified both.

**Chosen for `precedence`:** drop the field and reject the key with a message saying it is not
implemented. Silently ignoring it is the exact failure mode this package exists to remove, and
keeping an unread field contradicts the guideline against code with no current requirement. A
modder who has it in their file gets one clear error and deletes one line; today they get a
setting that does nothing and no way to find that out.

### [M] `PopTypeRegistry::Validate_` ignores internal references

`include/game/population/pop-types/PopTypeRegistry.h:27-45` — only the `is_default` count is
checked. Verified: a typo'd `fallback_pop_type` fails at `ConvertToFallback` runtime, and a bad
`obsoletes` entry is inert forever in `PopTypeAvailabilityCalculator::ResolveCurrentType`.

### [M] Rating parser hand-rolls its file load and reads fields weakly

`SocialRatingConfigParser.cpp:17-66` — a verbatim copy of `JsonConfigLoader::LoadFile` (which the
sibling policy parser already uses), plus `operator[]` for `id` and `levels`. Verified: a missing
`levels` yields an empty table rather than a load error.

### [M] A missing rating table silently drops the axis

`SocialRatingResolver.cpp:31-35` — a non-zero total whose axis is absent from the registry is
skipped. Verified. `ClampSocialRatingTotal` (`:82-87`) is also UB on an empty `levelEffects`:
it dereferences `begin()`/`rbegin()` with only a doc comment as the precondition.

### [M] `SocialScores` is a stub nothing references

`include/game/social-engineering/SocialEffects.h:6-18`. Verified — zero includes in the tree. It
presents a parallel per-field model beside the real `SocialRatingId_t` map accumulation.

### [M] `GameSettings` trusts a user-editable file

`GameSettings.cpp:14-30` — `width`, `height`, `oceanCoverage`, `presetId`, `seed` are read
verbatim. `WorldMap` now throws on non-positive dimensions (package 10), which turns a
hand-edited `"width": 0` into an exception from deep inside generation rather than a message
about the settings file. `Load` is the trust boundary and should say so.

### [M] Back-compat branch and layout drift in `GameSettings`

`GameSettings.cpp:32-57` — an explicit "older prefs" fallback, which the guidelines forbid and
which `Save` (`:132-142`) has never produced; plus `remove_shroud` living under `game_rules` and
`remove_fog` under `debug_options` "so existing user_settings.json files continue to load", so
the on-disk grouping no longer matches `VisibilityConfig_t`. Verified.

### [M] `k_GameCategoryCount` is a second source of truth

`include/game/GameCategory.h:19-26`. Verified: `ResearchSelector.h:43` sizes
`std::array<bool, k_GameCategoryCount>` and `ResearchSelector.cpp:43` indexes it by the cast enum
value, so adding a fifth category compiles and writes out of bounds. `k_AllGameCategories` has no
users at all.

## Review follow-ups

The review cleared the three things I was most worried about — it traced `config/tech_cost.lua`
and `config/pop_composition.lua` and confirmed both always return integers ≥ 1 (both end in
`math.floor`, and the tech formula in `math.max(1, ...)`), confirmed `GameSettings::Load` is
inside `main`'s try/catch, and could not break the new cycle detector. What it did find:

1. **[Regression] The missing-rating-axis throw fires mid-turn, not at load.** Swapping `Find`
   for `Get` in `SocialRatingResolver` made a modded policy naming an axis with no table kill the
   game on the first turn after the player adopts it — and `Registry::Get`'s message would say
   only `Unknown id 'police'`, naming neither the kind of id nor the policy. That inverts this
   package's whole premise. Fixed properly: `ValidateEffectReferences` now checks
   `SocialRatingModifierEffect_t` axes against the rating registry at load, naming the source;
   the resolver keeps a `Find` + explicit throw as a backstop with a message that says what it is.

2. **The new errors did not name the file, the id, or the key** — which is the package's stated
   contract. `at("cost")`, `at(key)` in the growth parser, and `at("levels")` all threw
   nlohmann's bare `key 'x' not found`, and `ParseStringArray` named the key but not the entry.
   All now name what a modder needs, and the tests assert the message rather than just the throw.

3. **`GameSettings::Load` still treated a wrong-shaped section as absent** — the exact defect
   this package fixes in `ParseStringArray`, left in the function the diff newly designates the
   trust boundary. `{"map_generation": "large"}` silently loaded defaults.

4. **Hygiene bullets from the research and social-engineering blocks that I had skipped without
   recording them as deferred**: two empty out-of-line constructors, an unused header include, a
   value parameter with an `r` prefix, and `ValidateNoDuplicates_`'s O(n²) scan (now derived from
   the index that is built immediately above it). Also removed `config/pop_growth.lua`, an unused
   leftover beside `pop_growth.json`.

5. **Dead branch in the cycle detector** (`marks[rId] == Done` at `nextPrereq == 0` is
   unreachable, since only `Unvisited` nodes are pushed), a fractional `nutrients_per_pop` still
   truncating silently, `user_settings.json` not migrated to the layout its own loader now
   expects, one test still constructing an empty-formula `TechCostConfig_t`, and temp files left
   behind by the new tests.

## Scope note

The package's fix direction mentions routing every single-object parser through a shared loader;
there are ten hand-rolled `std::ifstream` parsers in `src/`. Only the ones named in findings
(`GrowthConfigParser`, `SocialRatingConfigParser`) are converted here, plus the shared
`LoadObjectFile` helper they need. Converting the other eight is mechanical churn with no finding
behind it — Package 16's hygiene sweep is the place for it, and doing it here would bury the
behavioural changes above.
