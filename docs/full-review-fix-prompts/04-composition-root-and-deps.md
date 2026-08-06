# Package 4 — Composition root, dependency validity, session boundaries

**Source package:** [`docs/full-review-fix-packages.md`](../full-review-fix-packages.md), Package 4
**Verified against:** working tree at commit `418f681` (after full-review Packages 1–3)

---

## Verified diagnosis

Every claim below was re-checked at `path:line` against the current tree.

### [H] `GameDataContext` is a default-constructed bag of nullable pointers — CONFIRMED

`include/game/GameDataContext.h:45-83` declares 22 `std::unique_ptr` members plus
`std::vector` fields, with a defaulted constructor (`src/game/GameDataContext.cpp:39`). The
object only becomes usable after the free function `LoadGameData`
(`src/game/GameDataContext.cpp:42`) mutates every field in a documented-but-unenforced order.

Nothing in the type says which fields a consumer requires, so each consumer invents an answer:

- `src/game/Faction.cpp:56` dereferences `*rDataContext.moraleCalculator` unchecked — UB if unset.
- `src/game/Faction.cpp:51-58` passes five more as possibly-null `.get()` raw pointers.
- `tests/GameFixtures.h:56` builds one field-by-field, so the "half-loaded" state is routinely
  exercised and every downstream subsystem's null tolerance is load-bearing.

### [H] `Faction` is half-constructed until `GameState::AddFaction` finishes wiring — CONFIRMED

`src/game/GameState.cpp:180-195`: a `Faction` returned by its own constructor has no world map,
no settings, no `GameState` back-pointer and no observers. `AddFaction` then applies
`SetSettings`, `BindWorldMap`, `BindWorldEffects`, `BindGameState`, `SetOnBaseListChanged` and
`SetOnVisibilityRebuilt`.

The failure mode is silent rather than loud: `Faction::RebuildVisibility` early-returns on a
null `m_pWorldMap` (`include/game/Faction.h:286` records it as "set by BindWorldMap"), so a base
founded on an unwired faction produces no visibility, no territory rebuild and no first-contact
check while every getter still returns plausible values — the inverse of the project's
throw-on-unexpected-null rule.

**The wiring order is also load-bearing and undocumented:** `BindWorldMap` rebuilds visibility
*before* `SetOnVisibilityRebuilt` is installed and *before* the faction is pushed into
`m_factions`. A faction that arrives already populated (load-game, or any future runtime
creation) is therefore never scanned for contact or territory.

### [H] `BaseManager` takes ten nullable dependencies with three null behaviours — CONFIRMED

`include/game/faction/base/BaseManager.h:72-88` — ten pointer parameters. The class reacts
differently to each: `BuildBaseEffects_()` throws, a null `m_pSocialRatings` **silently skips
all social-rating expansion**, a null composition calculator makes `RecalculateComposition` a
no-op, and a null research/secret-project calculator surfaces as a throw from inside
`BuildingManager::GetBuildingsAvailableForConstruction` at whatever call site asks for a build
list.

This is not harness-only slack: `tests/GameFixtures.h:116-125` builds bases with a null rating
registry, null research manager, null composition calculator and null secret-project
calculator, so **fixture bases resolve social ratings to nothing while the real game resolves
them** — a behaviour difference with no diagnostic. That is the single most valuable thing this
package removes.

### [H] `PopContainer` / `PopulationManager` optional deps, one dereferenced unchecked — CONFIRMED

`src/game/faction/base/population/PopulationManager.cpp:27` seeds
`m_maxSize(pGrowthConfig ? pGrowthConfig->maxBaseSize : 7)` — declaring the config optional —
yet `GetNutrientsRequired` and `ApplyGrowth` dereference `*m_pGrowthConfig` unchecked, so the
"supported" null case is a crash on the first growth turn. The literal `7` also re-hardcodes
the population cap that `pop_growth.json` owns.

### [M] The remaining null-policy findings — CONFIRMED as the same defect

`ResourceManager` (six nullable pointers re-checked at every use), `FactionEffectsPool`,
`ResearchManager`, `ResearchSelector`, `SocialEngineeringManager`. Same shape, same fix.

---

## Chosen design

**One rule, applied everywhere: if the single composition root always supplies a dependency, the
constructor takes it as a reference.** Pointers survive only where absence is a real, documented
runtime mode — and then the class handles it exactly one way.

The composition root does supply all of these today, which is what makes the change mechanical
rather than speculative.

### A. `GameDataContext` becomes valid-by-construction

`LoadGameData` becomes a factory returning a fully-built context rather than a mutator of a
default-constructed bag. A completeness check runs at the end and throws naming the missing
field, so a partially-loaded context cannot escape the loader.

*Rejected:* holding every member by value. The members are forward-declared incomplete types and
several are polymorphic registries; by-value storage would force every consumer to include every
registry header, which is a worse coupling than the one being removed.

*Rejected:* reference accessors that throw on null (`BuildingRegistry& Buildings()`). That keeps
the locator shape and moves the failure to first use rather than to load.

### B. `Faction` is valid when its constructor returns

The constructor takes `WorldMap&` and `const GameSettings&` — both always available at every
construction site, including fixtures. `m_pWorldMap`'s silent early-return disappears because the
member becomes a reference.

The `GameState` back-pointer and the two observers stay a post-construction step, because they
are a genuine container↔element cycle: `GameState` owns the faction vector, and the observers
close over `GameState`. They move into **one** `AttachToSession_` step that `AddFaction` performs
*after* the faction is in `m_factions`, which fixes the load-bearing order bug: an
already-populated faction is now scanned for contact and territory.

*Rejected:* `Faction` taking `GameState&` in the constructor and `GameState::CreateFaction` being
the only mint point. It is the cleaner end state, but it forces a `GameState` into ~10 test sites
that deliberately construct standalone factions, and the cycle means the faction would observe a
`GameState` that has not finished constructing. Deferred to the god-facade follow-up.

### C. Leaf subsystems take references

`BaseManager`, `BuildingManager`, `PopContainer`, `PopulationManager`, `ResourceManager`,
`ResearchManager`, `ResearchSelector`, `SocialEngineeringManager`, `FactionEffectsPool`.

`pEffectsProvider` is the one dependency that stays a pointer: `Faction` passes `this` during its
own construction, and a standalone base legitimately has no provider.

Fixtures must then build real registries. **This is the point, not a cost** — it deletes the
fixture/game behaviour divergence called out above.

### D. Session boundaries

`Engine::Initialize_` splits into app-init (config load, graphics, input — once per process) and
new-game (world gen, factions, session state) phases, so `tests/GameFixtures.h` can share the
new-game path instead of reimplementing it. `main.cpp` gets top-level error handling.

---

## Out of scope (deferred, with rationale)

**[H] `GameState` and `Faction` are god-facades.** The package text says not to merge this unless
analysis finds a cheap seam. There is none: `GameState` exposes ~50 public members and `Faction`
~60, spanning eighteen unrelated concerns. Splitting them is a multi-package refactor that would
collide with every other open package. It needs its own package, sequenced after 5–17 land.

**`BaseSnapshot_t`'s untyped production id** — owned by package 6 (production contract).

---

## Test plan

- A `GameDataContext` missing a field fails at load with a message naming that field.
- A `Faction` cannot be constructed without a world map or settings (compile-time, by signature).
- A faction added to `GameState` **already holding bases** is scanned for territory and first
  contact — the ordering bug, which no existing test pins.
- Fixture bases resolve social ratings the same way real bases do (the divergence closes).
- Existing suites must keep passing: they are the regression net for the constructor churn.
