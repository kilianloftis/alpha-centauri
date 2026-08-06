## Game core — world state and composition root

**Files:** `src/main.cpp`, `src/game/GameState.cpp`, `include/game/GameState.h`,
`src/game/Faction.cpp`, `include/game/Faction.h`, `src/game/GameDataContext.cpp`,
`include/game/GameDataContext.h`, `src/game/GameSettings.cpp`, `include/game/GameSettings.h`,
`src/game/GameCategory.cpp`, `include/game/GameCategory.h`, `include/game/GameDataPaths.h`,
`include/game/GameRulesConfig.h`, `include/game/IConstructable.h`,
`include/game/IEffectsProvider.h`, `include/game/VisibilityConfig.h`

**Assessment:** Lifetime management in this slice is genuinely good: member declaration order
in `GameState`/`Engine` is deliberate and documented, `DerefView` keeps the owning
`unique_ptr`s private, and `IEffectsProvider` / `GameDataPaths` / `VisibilityConfig_t` are
small, honest abstractions. The dominant weakness is that both central classes have grown
into facades that every new subsystem must edit, and that object validity is still
established in stages after construction — `GameDataContext` is default-constructed empty
and filled by a free function, and `Faction` is re-wired in four steps by
`GameState::AddFaction`. The small config/enum files are clean apart from two hand-maintained
duplicates and one back-compat branch the guidelines forbid.

### [H] `GameDataContext` is still a service locator: a default-constructed bag of nullable pointers
`include/game/GameDataContext.h:46` — the struct declares 22 default-null `unique_ptr`
members and a defaulted constructor; the object only becomes usable after the free function
`LoadGameData` (`src/game/GameDataContext.cpp:42`) mutates every field. Nothing in the type
expresses which fields a given consumer requires, so each consumer invents its own answer:
`Faction`'s constructor dereferences one unchecked (`src/game/Faction.cpp:55`,
`*rDataContext.moraleCalculator` — undefined behavior if unset) while passing four others as
possibly-null `.get()` raw pointers (`src/game/Faction.cpp:50-58`), and
`Faction::GetDiscoveredBuildings` throws on a null registry
(`src/game/Faction.cpp:516`). Partially-populated contexts are not hypothetical:
`tests/GameFixtures.h:69` builds one field by field, so the unspecified "half-loaded" state is
routinely exercised and the null-tolerance of every downstream subsystem is load-bearing.
Prior finding 1.3 was closed only for the leaf consumers (`BaseManager` etc.); the root object
was left as-is, so the seam is still a locator rather than an injection point. Direction: make
`LoadGameData` a factory that returns a fully constructed context (or take `GameDataPaths` in
the constructor) and hold the members by value/reference so "null" is unrepresentable.

### [H] `Faction` is only half-constructed until `GameState::AddFaction` finishes wiring it
`src/game/GameState.cpp:145-181` — a `Faction` returned by its own constructor has no world
map, no settings, and no observers; `AddFaction` then applies `SetSettings`, `BindWorldMap`,
`SetOnBaseListChanged`, `SetOnVisibilityRebuilt` and two signal connections. The failure mode
is silent, not loud: `Faction::RebuildVisibility` returns early when `m_pWorldMap` is null
(`src/game/Faction.cpp:596`), so a base founded on an unwired faction produces no visibility,
no territory rebuild and no first-contact check while every getter still returns plausible
values — the opposite of the project's "throw on unexpected null" rule. The wiring order is
also load-bearing and undocumented: `BindWorldMap` (which rebuilds visibility) runs before
`SetOnVisibilityRebuilt` is installed and before the faction is pushed into `m_factions`, so a
faction that arrives already populated (load-game, or any future runtime creation) is never
scanned for contact or territory. This is a fresh instance of prior finding 4.2, which was
recorded as fixed for `Faction`/`GameState`. Direction: take the map, settings and the
`GameState` back-reference in `Faction`'s constructor, and make `GameState` the only thing
that can mint one.

### [H] `GameState` and `Faction` are god-facades that every new subsystem must edit
`include/game/GameState.h:40-207` declares roughly fifty public members spanning mission year,
settings, event bus, faction registry, diplomacy, ID allocation, world map, base lookup,
combat interception, base conquest, effect collection, pathfinding, order execution, probe
actions, first contact, council, territory, secret projects and orbital warfare. Several are
pure forwarders that add only `*this` and `m_rng` to a free function
(`src/game/GameState.cpp:429-452`, `308-325`). `include/game/Faction.h:52-258` has the same
shape at ~60 methods: it owns eight subsystems and *also* keeps its own ad-hoc state and rules
inline — the ASAT/intercept cooldown vector (`include/game/Faction.h:250-258`) and a
faction-wide building inventory (`CountBuildings`, `FindBaseWithBuilding`,
`FindOwnedBuildingConfig`, `CountReadyBuildings`) that belongs in a subsystem next to
`Military`/`BuildingManager`. Every feature added so far has been added by growing these two
headers, which is a direct OCP violation at the busiest merge point in the codebase.

### [H] Ownership transfer is implemented as destroy-then-recreate, inheriting destruction's side effects
`src/game/Faction.cpp:341-397` — `ExtractUnit` routes through `UnitManager::DestroyUnit`
(`src/game/faction/UnitManager.cpp:71`), which applies carrier-loss rules to the unit's cargo
(disembark if the tile is holdable, otherwise destroy) and emits `OnUnitDestroyed`;
`UnitSnapshot_t` (`include/game/units/Unit.h:25-34`) carries no carrier or cargo link. So
subverting a loaded transport (`src/game/units/ProbeActionEffects.cpp:180`) applies
"ship sunk" semantics to a ship that was not sunk, and every observer — including
`GameState`'s revealed-unit cleanup (`src/game/GameState.cpp:167`) and any mod on the event
bridge — sees "destroyed" followed by "created" rather than a change of owner. Both transfer
paths are also destructive on failure: `TransferUnitTo` (`src/game/Faction.cpp:383`) and
`TransferBaseTo` (`src/game/Faction.cpp:322`) have already extracted (and thus destroyed) the
object when `CreateFromSnapshot` throws, with no rollback. Direction: give `UnitManager` a
detach-without-destroy path, or make the snapshot round-trip complete (carrier/cargo) and
non-destructive.

### [M] Building deploy cooldowns are never pruned and leak across base transfer
`src/game/Faction.cpp:192-209` — `DeployBuilding` appends a record that nothing ever removes
on expiry, so `m_buildingDeploys` grows for the whole game and `CountReadyBuildings`
(`src/game/Faction.cpp:178`) rescans it every query. `NotifyBuildingDestroyed`'s comment says
it prefers dropping a "still cooling" record, but its predicate matches on `buildingId` only,
so it usually erases a long-expired record instead — the code does not do what the comment
promises. Worse, `ExtractBase`/`TransferBaseTo` do not drop the records for buildings that
left with the base, so a faction that loses an ODP-bearing base keeps a phantom cooldown that
will suppress a rebuilt copy years later. The root cause is that per-copy cooldown state is
keyed by building id with no per-instance identity and is parked on `Faction`.

### [M] `main.cpp` has no top-level error handling
`src/main.cpp:4-9` — `LoadGameData` and every config parser throw by design (this is the
project's chosen failure mode), and both `Engine`'s constructor and `Run()` propagate. With no
`try`/`catch` in `main`, the single most likely user-visible failure of a moddable,
config-driven game — a typo in a JSON or Lua file — produces `std::terminate` and an abort
with no message. Wrapping `Run()` and reporting `what()` with a non-zero exit code is a
three-line fix in the one place that should own it.

### [M] `GameSettings` loads world-generation values from a user-editable file without validating them
`src/game/GameSettings.cpp:14-30` — `width`, `height`, `oceanCoverage`, `presetId` and `seed`
are taken verbatim from `user_settings.json` with no range or referential check, even though
`MapGenerationConfig_t` documents `oceanCoverage` as `[0,1]`
(`include/game/map/MapGenerationConfig.h:27`). `Engine` feeds these straight into `WorldMap`,
whose constructor does not validate either (`src/game/map/WorldMap.cpp:8`), so a hand-edited
`"width": 0` or `-1` becomes a degenerate or allocation-failing world rather than a clear
error. `Load` is the trust boundary and should throw here.

### [M] Back-compat branch and struct/file layout drift in `GameSettings`
`src/game/GameSettings.cpp:32-57` — `LoadGameRules_` keeps a fallback for "older prefs [that]
stored pause at the top level", and `LoadVisibility_` deliberately reads `remove_shroud` from
`game_rules` and `remove_fog` from `debug_options` "so existing user_settings.json files
continue to load". The guidelines explicitly forbid keeping code for backwards compatibility,
and `Save` (`src/game/GameSettings.cpp:132-142`) has never written the top-level form, so that
branch is reachable only for files no current build produces. The lasting cost is the second
half: the on-disk grouping no longer matches `VisibilityConfig_t`, so the next visibility knob
has a non-obvious home and the Save/Load pair must be edited in lockstep to stay consistent.

### [M] `k_GameCategoryCount` is a hand-maintained duplicate of the enum
`include/game/GameCategory.h:19-26` — the count and the `k_AllGameCategories` array restate
the enumerators by hand while the `.cpp` already uses `magic_enum` for everything else.
`ResearchSelector` sizes `std::array<bool, k_GameCategoryCount>` and indexes it by the cast
enum value (`include/game/faction/ResearchSelector.h:40`,
`src/game/faction/ResearchSelector.cpp:43`), so adding a fifth category compiles cleanly and
writes out of bounds. `magic_enum::enum_count<GameCategory_t>()` removes the second source of
truth; `k_AllGameCategories` has no users at all and should just go.

### [M] The `IUnitOrderWorld` overrides hand `GameState` its own RNG and tile-effects back
`src/game/GameState.cpp:308-325` — `TryInterceptAttack` and the two conquest resolvers receive
`std::mt19937&` and `TileEffectsContext&` parameters and forward them, but `GameState` already
owns both (`m_rng`, `m_pTileEffects`) and its sibling `TryAttackSatellite`
(`src/game/GameState.cpp:445`) reaches for `m_rng` directly. Two routes to the same state mean
a caller that supplies a different generator silently forks the deterministic roll stream
without any compile error. Each override also shares its exact name with the free function it
delegates to, so dropping the `ac::` qualifier turns the call into unbounded recursion.
Removing the parameters requires a matching edit in `include/game/units/IUnitOrderWorld.h`.

### [L] Convention and hygiene items
- `src/game/GameState.cpp:256` — `AllocateBaseId` is defined returning `int` while declared as
  `BaseId_t`; use the alias in both places.
- `include/game/GameState.h:185` — `m_worldMap` is the only owning pointer member without the
  `p` prefix every sibling uses (`m_pTileEffects`, `m_pPathfinder`, …).
- `include/game/GameSettings.h:19` — `kDefaultPath` should be `k_DefaultPath` per the `k_`
  constant convention.
- `src/game/GameState.cpp:311` vs `:442` — `ac::` and `::ac::` qualification used
  interchangeably in the same file.
- `src/game/Faction.cpp:62` — `~Faction() {}` with an empty body where `GameState` uses
  `= default` for the identical out-of-line-destructor purpose.
- `src/game/Faction.cpp:108`, `:216`, `:462`, `:475` — `if (pBase)` guards over a vector whose
  only insertion point (`AddBase`, `src/game/Faction.cpp:236`) already throws on null; the
  neighbouring loops that use `Bases()` have no such guard.
- `src/game/Faction.cpp:455-481` — `ProduceBaseResources` and `ApplyBaseGrowth` are identical
  apart from the per-base call; likewise five near-duplicate "sum over bases" loops split
  between raw `m_bases` and `Bases()`.
- `src/game/GameCategory.cpp:33-45` — `ParseGameCategory` hand-rolls case-insensitive matching
  and lowercases every enumerator name on every iteration;
  `magic_enum::enum_cast<GameCategory_t>(s, magic_enum::case_insensitive)` is the one-liner.
- `src/main.cpp:1` — `<iostream>` is included and unused.
- `src/game/Faction.cpp:1-33` — the include block puts the file's own header second and
  interleaves `<algorithm>` / `<iostream>` among the game headers.
- `src/game/Faction.cpp:447` — `CreateBase` writes to `std::cout` from domain logic; this adds
  to the known logging-seam debt recorded as deferred in the prior review, not a new problem.
- `include/game/IConstructable.h:21` — the method is `GetBaseCost()`, but
  `docs/architecture/high-level.md:269` and `docs/architecture/faction-system.md:259` both
  still document it as `GetMineralCost()`.

**Observed outside slice:**
- `src/game/units/MovementRules.cpp:135` — unit placement legality depends on a mutable
  file-scope global (`s_bSingleUnitPerTile`) rather than config or injected rules.
- `src/game/map/WorldMap.cpp:8` — the constructor accepts any width/height, including zero and
  negative, and `reserve(width * height)` on a negative product is a trap.
