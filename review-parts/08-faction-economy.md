## Faction — economy, research, social engineering, identity

**Files:** `{src/game,include/game}/faction/` — `EconomyManager.{cpp,h}`,
`ResearchManager.{cpp,h}`, `ResearchSelector.{cpp,h}`, `SocialEngineeringManager.{cpp,h}`,
`FactionEffectsPool.{cpp,h}`, `Specialist.{cpp,h}`, `AIProfile.{cpp,h}`,
`FactionConfigParser.{cpp,h}`, `FactionFlavor.{cpp,h}`, `FactionIdentity.{cpp,h}`,
`FactionConfig.h`, `FactionRegistry.h`

**Assessment:** `FactionEffectsPool` is the strongest piece here and its **invalidation is
complete**: I traced every mutator of every contributor — `BuildingManager::AddBuilding` /
`DestroyBuilding`, `PopContainer::AddPop` / `RemovePop` / `ConvertTo` / `ConvertToFallback`,
`UnitManager::CreateUnit` / `DestroyUnit`, `SocialEngineeringManager::SetActivePolicy`,
`Faction::AddBase` / `ExtractBase`, `ResearchManager::AddDiscoveredTech` — and each bumps a
`Revision` that `CollectRevisions_` stamps. `Unit`'s design is a `const&` member so unit
effects cannot change in place, and `UnitManager::Units()` filters the null slots left by
deferred destruction, so a rebuild during a `DeferredDestructionScope` is safe. The defects
are elsewhere: `Rebuild_` interleaves *expansion* passes with *collection* passes so two
kinds of effect are consumed before their producers exist or after their gate is lifted, and
the cache is not bound to the faction it was built for. Outside the pool, the dominant
weakness is optional-pointer dependencies handled three different ways per class, plus a
research cost cache keyed on only one of its two inputs. `EconomyManager`, `FactionIdentity`,
`AIProfile` and `Specialist` are thin to the point of being liabilities.

### [H] Research cost cache is keyed on the effects version but also depends on tech count
`src/game/faction/ResearchManager.cpp:116-118,131-141` — `ComputePointsNeeded_` feeds
`m_discoveredTechs.size()` into `TechCostInputs_t::techs`/`mostTechs`, but
`RevalidatePointsNeeded_` only compares `m_pEffectsProvider->GetEffectsVersion()` against
`m_costEffectsVersion`, and returns early when there is no provider. `AddDiscoveredTech` is
called mid-research from three production paths that never touch the target
(`src/game/units/ProbeActionEffects.cpp:72` tech steal,
`src/game/faction/DiplomaticActionExecutor.cpp:319` treaty tech, `Engine.cpp:322`); each
changes a direct input to the cost. Today it happens to work because
`FactionEffectsPool::CollectRevisions_` stamps the research revision, so the pool version
moves and drags the cost cache with it — the cost cache is correct only as a side effect of
the *effects* cache depending on research. Take `removed_by_tech` out of the pool, or
construct with `pEffectsProvider == nullptr` (a documented, supported configuration,
`ResearchManager.h:18-19`), and the cost silently freezes at its value from
`SetResearchTarget`. The class already owns `m_revision` and bumps it in
`AddDiscoveredTech`; the fix is to stamp and compare that alongside the provider version.

### [H] `EconomyManager` owns the treasury but offers no way to spend from it
`src/game/faction/EconomyManager.cpp:12-15` — `AddEnergy` is the only mutator and applies
any signed amount unchecked, so spending is written as `AddEnergy(-cost)` at four call sites
(`src/game/units/ProbeActionExecutor.cpp:152`, `src/game/units/UnitOrderExecutor.cpp:364`,
`src/game/units/ProbeActionEffects.cpp:99`, `src/game/faction/DiplomaticActionExecutor.cpp:314`)
and the "can I afford it" rule is re-implemented at three more
(`ProbeActionExecutor.cpp:148`, `src/game/units/TerraformRules.cpp:161`,
`DiplomaticActionExecutor.cpp:226`). All four sites currently check, so nothing is broken
today, but the invariant "the treasury never goes negative" lives in the callers rather than
in the class that owns the resource, and the fifth spender will be the one that forgets —
with no bankruptcy rule, the result is a silently negative treasury. A `SpendEnergy(int)`
that throws (or a `CanAfford`/`TrySpend` pair) would make the rule unforgeable and delete
three duplicated checks.

### [M] Social-rating expansion runs before pop and unit effects are collected
`src/game/faction/FactionEffectsPool.cpp:158-167` — `ExpandFactionLaneSocialRatingEffects`
accumulates `SocialRatingModifier` totals from whatever is in `factionEffects` *at that
point*: tile-yield rules, definition effects, buildings and policies. `CollectPopEffects_`
and `CollectUnitEffects_` append afterwards, so a faction-lane rating modifier declared by a
pop type or a unit component never reaches the faction-lane rating level. The same modifier
*does* count at base level, because `BaseManager::BuildBaseEffects_` runs
`ExpandSocialRatingEffects` over a list that already includes pop and unit contributions —
so the two lanes would disagree about the faction's Economy rating. No shipped config
declares a rating modifier on a pop type or unit component yet (only
`config/social_policies.json`, `config/factions/gaian/effects.json` and buildings do), so
this is latent rather than live. Move the expansion below every collection pass.

### [M] `removed_by_tech` is filtered after the effects it gates have already been expanded
`src/game/faction/FactionEffectsPool.cpp:169-178` — the `removedByTech` filter is the last
statement in `Rebuild_`, but grant expansion happened at line 62
(`ExpandGrantBuildingEffects`) and rating expansion at line 160. An effect that both carries
`removed_by_tech` and produces derived effects — a `GrantBuilding`, or a
`SocialRatingModifier` — has its expansion baked in before the gate is applied, so
discovering the tech removes the gate effect while everything it expanded into stays in the
pool. Only `config/tile_yield_rules.json` uses the field today (three
`TileResourceCap` entries, appended first and never expanded), so this is not currently
reachable; but `removedByTech` is parsed for every effect
(`src/game/effects/BonusEffectParser.cpp:329`) and documented as a general facility
(`include/game/effects/BonusEffect.h:399-401`). Filter before expanding.

### [M] The memoized pool is not bound to the faction it was built for
`src/game/faction/FactionEffectsPool.cpp:31-41` — `Get`/`GetVersion` take
`const Faction&` per call and the cache stores no identity, so validating with a different
faction returns the *other* faction's effects whenever the two happen to produce an equal
revision stamp — trivially true for two freshly created factions, where every contributor
revision is 0 and neither has bases. The header (`FactionEffectsPool.h:15-19`) presents this
as a deliberate design ("the pool holds no back-reference to its owner"), but the pool is a
by-value member of `Faction` and the only caller passes `*this`
(`src/game/Faction.cpp:628-636`), so the flexibility buys nothing and costs the invariant.
Take `const Faction&` in the constructor.

### [M] Optional dependencies with three different null policies per class
The four constructor pointers of `FactionEffectsPool` are all nullable, two with `= nullptr`
defaults (`FactionEffectsPool.h:23-26`), and each is handled by silently skipping work — a
null building registry returns the building effects **unexpanded**
(`FactionEffectsPool.cpp:52-55`), so every `GrantBuilding` chain vanishes with no diagnostic.
The same pointer inside one class is treated
three different ways elsewhere: `ResearchManager` dereferences `m_pTechRegistry` unchecked in
`SetResearchTarget` (`ResearchManager.cpp:32`), throws on it in `RecalculatePointsNeeded`
(`:107-110`), and returns `{}` for it in `GetAvailableTechs` (`:196-199`);
`ResearchSelector` throws on a null manager in `GetCandidateTargets`
(`ResearchSelector.cpp:52-56`), returns `false` in `AssignResearchTarget` (`:88-91`) and
returns silently in `EnsureResearchTarget` (`:110-113`). The guidelines call for constructors
that produce valid objects and for throwing on unexpected null; the single composition root
(`src/game/Faction.cpp:50-58`) always supplies every one of these, so they can all become
references.

### [M] Hardcoded default policy ids now hard-fail instead of silently failing
`src/game/faction/SocialEngineeringManager.cpp:16-19,42-52` — the four starting policies are
still compiled-in string literals (`"frontier"`, `"simple"`, `"survival"`, `"none_future"`).
This is the fix recorded for prior finding 1.11, and it is incomplete in the direction the
guidelines care about: validating the hardcoded ids converts "mod ships different starting
policies → `GetActivePolicy` returns nullptr forever" into "mod ships different starting
policies → every faction constructor throws and the game will not start". The ids belong in
config (a `default: true` flag per category in `social_policies.json`, or a
`starting_policies` block), validated the same way. The `if (!m_pRegistry) return;` at
`:30-33` also skips the whole check, leaving a manager with no policies at all — a fourth
null policy in a class that throws on a null registry two methods later (`:61-64`).

### [M] `GetSocialRating` recollects and re-accumulates the whole rating map per query
`src/game/faction/SocialEngineeringManager.cpp:100-105` — every call runs `CollectEffects()`
(a fresh vector with a `std::string sourceId` per effect) and `AccumulateSocialRatings` (a
fresh `std::map`) to answer a question about *one* axis. `SocialEngineeringDisplay` calls it
once per rating axis per frame (`src/ui/social-engineering/SocialEngineeringDisplay.cpp:120,412`),
so this is ten vector+map allocations per frame — the one un-memoized read left on the SE
side after the pool work. The class already owns the `Revision` it would need to cache the
accumulated map against.

### [M] The rebuild stamp is re-collected after the rebuild instead of snapshotted before it
`src/game/faction/FactionEffectsPool.cpp:133,181` — `Validate_` collects the current
revisions into `m_scratchRevisions`, then `Rebuild_` walks every base a second time to
produce `m_cachedStamp`. Nothing in the collection path mutates a contributor today, so it is
currently equivalent, but stamping *after* reading means any future collector with a side
effect (a Lua hook, a lazily-materialised design) records a stamp newer than the content and
the cache stays permanently stale with no way to notice. `m_cachedStamp = m_scratchRevisions;`
is correct by construction and drops a full traversal.

### [M] `FactionIdentity` is a bypassed duplicate of the config it copies
`src/game/faction/FactionIdentity.cpp:6-13` copies six strings out of `FactionIdentityConfig`
and `LeaderConfig` into a heap-allocated object (`src/game/Faction.cpp:45`) whose only
consumer in the whole codebase is `FactionFlavor` — `Faction` exposes no `GetIdentity()` at
all. Every other reader goes to the config directly: eleven sites use
`GetDefinition().identity.name` (e.g. `src/game/council/CouncilFactionVotesPanel.cpp:52,117`,
`src/ui/commlinks/CommlinksPanel.cpp:52`), and the two identity fields that carry real rules —
`participatesInCouncil` and `species` — are not on `FactionIdentity` at all, so
`src/game/units/BaseConquestEffects.cpp:292` and `src/game/council/PlanetaryCouncil.cpp:46`
have to bypass it. The result is two representations of faction identity where the
authoritative one is the config. Give `FactionFlavor` the `FactionIdentityConfig` and
`LeaderConfig` directly and delete the class.

### [M] Flavor RNG cannot be seeded, so base names are not reproducible
`src/game/faction/FactionFlavor.cpp:31` — the only constructor seeds `m_rng` from
`std::random_device`, and unlike `ResearchSelector` (which offers
`ResearchSelector(pManager, uint32_t seed)`, `ResearchSelector.cpp:34-39`) there is no seeded
overload. Base names are persistent game state, so this makes a run unreproducible from its
inputs and leaves `tests/faction/FactionFlavorTests.cpp` unable to assert which name is
picked. Two RNG-seeding policies inside one slice is also a coin-flip for the next
subsystem; a single game-level seed source would settle both.

### [L] Convention and hygiene items
- `include/game/faction/FactionConfig.h:19,33,38,46` — `FactionIdentityConfig`,
  `LeaderConfig`, `AITendenciesConfig`, `FactionFlavorConfig` are data structs and need the
  `_t` suffix (`FactionConfig_t` and `EnergyAllocation_t` in the same slice have it).
- `include/game/faction/Specialist.h` / `src/game/faction/Specialist.cpp` — a header with
  nothing but a comment and a `.cpp` that includes only itself, compiled into `ac-core`
  (`src/CMakeLists.txt:55`). Nothing includes the header. Delete both.
- `src/game/faction/AIProfile.cpp` — `AIProfile`'s four `InterestedIn*` getters have zero
  callers and `Faction::m_pAIProfile` is never read; the guideline is "no getters without an
  immediate requirement".
- `src/game/faction/FactionConfigParser.cpp:26-31` — `ParseFactionSpecies_` is a hand-rolled
  string→enum map living in the parser; the guideline puts the one map next to the enum
  (`FactionConfig.h:12-17`), and `magic_enum` is already used elsewhere in the project.
- `src/game/faction/FactionConfigParser.cpp:168-188` — `ReadJsonFile` has exactly one caller,
  `ReadRequiredJsonFile`, whose `fs::exists` check duplicates the `is_open` failure below it.
  Also `std::cout` progress logging in a parser at `:39,70`.
- `src/game/faction/ResearchSelector.cpp:14-25` — `CategoryIndex_` is a second source of
  truth for category ordering next to `k_AllGameCategories` (`include/game/GameCategory.h:21`);
  `magic_enum::enum_index` removes it.
- `src/game/faction/ResearchSelector.cpp:66-70,99-103` — two dead guards:
  `ResearchManager::GetAvailableTechs` only ever pushes `&rConfig` (`ResearchManager.cpp:223`)
  and `PickRandom_` cannot return null.
- `src/game/faction/ResearchManager.cpp:230-233` — `ResetAccumulatedPoints_` is declared,
  defined and never called.
- `src/game/faction/ResearchManager.cpp:172-182` — `TechId` is `std::string`;
  `HasDiscoveredTech(TechId)` takes it by value and the range-`for` copies every element it
  scans. Called per prerequisite in `GetAvailableTechs` and per effect in the pool's
  `removed_by_tech` filter. Same for `SetResearchTarget` and `AddDiscoveredTech`.
- `include/game/faction/FactionEffectsPool.h:33` — "changes iff the pool content changed" is
  not true: any contributor bump rebuilds and increments the version even when the content is
  identical (add then destroy an effect-less building), needlessly invalidating
  `BaseManager::BuildBaseEffects_` and the research cost.
- `include/game/faction/FactionEffectsPool.h:44-46` — the unit collector comment says "any
  scope except the locally-resolved ThisUnit/ThisTile", but `IsFactionLane` also drops
  `ThisBase` and `ProducedAtThisBase`, which parse-time validation permits on a unit
  component; those are silently discarded.
- `include/game/faction/SocialEngineeringManager.h:25-26` — the comment says `SetActivePolicy`
  throws "when a registry is bound"; it now also throws when one is not
  (`SocialEngineeringManager.cpp:61-64`). It also takes a whole `SocialPolicyConfig_t` only to
  read `.id` and look it up again — the id is the narrower parameter.
- `include/game/faction/FactionFlavor.h:24-25` — two reference members with no stated
  lifetime requirement; they alias registry-owned config that must outlive the faction.
- `src/game/faction/FactionFlavor.cpp:56,76-81` — the fallback base name (`"<Adjective> Base
  N"`) and the six substitution tokens are compiled-in English.
- Out-of-line empty/defaulted special members that could be `= default` in the header:
  `EconomyManager.cpp:8-10`, `ResearchManager.cpp:26-28`, `AIProfile.cpp:6-17`,
  `FactionIdentity.cpp:16-18`, `SocialEngineeringManager.cpp:55-57`.
- Reference parameters missing the `r` prefix throughout
  `FactionConfigParser.cpp` (`j`, `configPath`, `dirPath`, `filePath`) and
  `FactionFlavor.h:20` (`category`).
- `include/game/faction/FactionRegistry.h:10` — an empty subclass of `Registry<>` that exists
  only so `GameDataContext.h:19` can forward-declare it; needs a comment saying so.

**Observed outside slice:**
- `include/game/faction/base/population/PopContainer.h:33` + `include/game/population/pop-types/Pop.h:56` — `Pops()` hands out mutable `Pop&` and `Pop::Convert` is public, so a pop's type (a pool input) can be changed without bumping `PopContainer::m_revision`; only `PopContainer` calls it today, so the "every mutator bumps" invariant holds by discipline alone.
- `src/game/Faction.cpp:455-467,469-481` — `ProduceBaseResources`/`ApplyBaseGrowth` append external/council effects to a *copy* of the pool, while `BaseManager`'s cached `BuildBaseEffects_()` path sees the pool without them, so `GetEconProduction()` and the per-turn production run answer differently for the same base.
- `src/game/faction/base/BaseManager.cpp:300-310` — `BuildBaseEffects_()` calls `GetActiveEffects()` and `GetEffectsVersion()` back to back, validating the pool (a full per-base revision traversal) twice per stat read.
- `src/game/effects/BonusEffectParser.cpp:668-680` — `ValidateScopeForSource` only rejects `ThisPop` and `ThisUnit` on the wrong source kind, so a `ThisBase` effect on a unit component passes validation and is then silently dropped by every collector.
- `docs/architecture/faction-system.md:31-39,205-231` — the economy/AI subsystems are documented with members that do not exist (`Credits`, `TradeRoutes`, `IncomeCalculator`, `Personality`, `Priorities`, `UnitFactory`), and `FactionEffectsPool`, `ResearchSelector`, `SocialEngineeringManager` and `UnitManager` are absent entirely; `docs/architecture/high-level.md:285` lists the same stale subsystem set.
