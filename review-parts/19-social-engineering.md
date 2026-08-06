## Social engineering — policies and ratings

**Files:** `src/game/social-engineering/SocialPolicyConfigParser.cpp`, `include/game/social-engineering/SocialPolicyConfigParser.h`, `src/game/social-engineering/SocialRatingConfigParser.cpp`, `include/game/social-engineering/SocialRatingConfigParser.h`, `src/game/social-engineering/SocialRatingResolver.cpp`, `include/game/social-engineering/SocialRatingResolver.h`, `include/game/social-engineering/SocialEffects.h`, `include/game/social-engineering/SocialRatingRegistry.h`, `include/game/social-engineering/SocialPolicyRegistry.h`, `include/game/social-engineering/SocialRatingConfig.h`, `include/game/social-engineering/SocialPolicyConfig.h`

**Assessment:** Policy config + `IsAvailable`, registries, and the SMAC clamp path (`ClampSocialRatingTotal` / `FindSocialRatingLevelEffects` / per-base `ExpandSocialRatingEffects`) are clear and match the two-level design. The weak spot is faction-lane expansion: it accumulates on the raw faction pool and breaks the same invariant the per-base path and the resolver header document. Rating load also lagged behind the shared `JsonConfigLoader` / `ConfigFields` pattern the policy parser already uses.

### [H] Do not accumulate ThisBase modifiers for FactionUnits expansion
`src/game/social-engineering/SocialRatingResolver.cpp:91-95` — `ExpandFactionLaneSocialRatingEffects` calls `AccumulateSocialRatings` on the entire faction pool. That pool still holds every base’s `ThisBase` `SocialRatingModifier`s (`FactionEffectsPool::CollectBuildingEffects_`). The resolver’s own contract (`SocialRatingResolver.h:17-22`) says accumulation is only meaningful on a context-filtered list; per-base expansion honors that after `FilterForBase`, but the faction-lane path does not. Axes whose level tables emit `FactionUnits` effects (morale, probe teams, etc.) will then treat N bases’ local modifiers as one faction total and apply the wrong unit bonuses. Production data currently keeps Those modifiers FactionGlobal-only, but fixtures already use `ThisBase` Growth and the architecture advertises base-local rating mods — so the bug is latent and mod-facing. Fix: accumulate only FactionWide-lane modifiers (or otherwise exclude `EffectLane_t::Base`) before expanding FactionUnits gameplay effects; add a regression that pairs `ThisBase` morale/probe with multiple bases.

### [M] Rating parser still hand-rolls file load and weak field access
`src/game/social-engineering/SocialRatingConfigParser.cpp:17-66` — `ParseConfig` duplicates `JsonConfigLoader::LoadFile` (open / array check / loop / cout), while `SocialPolicyConfigParser` already uses the shared helper. `ParseRatingConfig` uses `operator[]` for `id` / `levels` instead of `ConfigFields::ParseId` and `.at("levels")`, so a missing `levels` object can yield an empty table instead of a load error. Route through `JsonConfigLoader::LoadFile` + `ConfigFields` and require `levels` to be a JSON object.

### [M] Clamp/lookup logic is copied three ways
`src/game/social-engineering/SocialRatingResolver.cpp:38-53`, `55-88`, `91-128` — `FindSocialRatingLevelEffects` already clamps and exact-matches, but both expand functions reimplement that path and nearly duplicate each other (differing only in the append filter). A future clamp or sourceId change can drift between UI lookup and gameplay expansion. Drive both expands through `FindSocialRatingLevelEffects` (plus a small append helper with an optional lane filter).

### [M] Missing rating-table entry silently drops the axis
`src/game/social-engineering/SocialRatingResolver.cpp:67-70`, `104-107` — a non-zero accumulated total whose axis is absent from `SocialRatingRegistry` (or has an empty `levelEffects`) is skipped with no error. Modifier parse already constrains `SocialRatingId_t`; a missing table row is a config/registry defect. Prefer `Get` / throw on unknown axis when `total != 0`, consistent with the project’s required-id rule.

### [M] Delete unused `SocialScores` stub type
`include/game/social-engineering/SocialEffects.h:6-18` — `SocialScores` is never included or referenced anywhere in the tree. It presents a parallel per-field score model next to the real `SocialRatingId_t` + map accumulation path and will mislead the next reader. Remove the header (or replace it only when a real DTO is needed).

### [L] Convention and hygiene items
- `include/game/social-engineering/SocialEffects.h:6` — `SocialScores` omits the `_t` data-struct suffix required by coding guidelines.
- `include/game/social-engineering/SocialPolicyRegistry.h:15` — parameter `rCategory` is passed by value; `r` prefix is for references.
- `include/game/social-engineering/SocialPolicyConfigParser.h:4` — `#include "game/effects/BonusEffectParser.h"` is unused in the header (only the `.cpp` needs it).
- `include/game/social-engineering/SocialPolicyConfigParser.h:15-16`, `SocialRatingConfigParser.h:14-15` — empty public default constructors add nothing; prefer `= default` on the declaration or omit.
- `src/game/social-engineering/SocialRatingResolver.cpp:31-35` — `ClampSocialRatingTotal` is UB on empty `levelEffects` (`begin()`/`rbegin()`); public API documents the precondition but does not enforce it (assert or throw).

**Observed outside slice:**
- `docs/architecture/effects-system.md:324-327` — still claims accumulation takes `BaseEffects_t` so the raw pool is a compile error; code now takes `vector<ActiveEffect_t>` and `ExpandFactionLaneSocialRatingEffects` runs on the pool (`FactionEffectsPool.cpp:160`).
- `docs/architecture/high-level.md:365` — still says `EffectConfig_t` “will eventually” live on `SocialPolicyConfig_t`; policies already store `effects`.
