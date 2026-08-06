## Faction — military, units, diplomacy, visibility

**Files:** `src/game/faction/Military.cpp`, `include/game/faction/Military.h`,
`src/game/faction/UnitManager.cpp`, `include/game/faction/UnitManager.h`,
`src/game/faction/UnitVisibility.cpp`, `include/game/faction/UnitVisibility.h`,
`src/game/faction/VisibilityRules.cpp`, `include/game/faction/VisibilityRules.h`,
`src/game/faction/FactionVisibleMap.cpp`, `include/game/faction/FactionVisibleMap.h`,
`include/game/faction/FactionExploredMap.h`, `include/game/faction/FactionRevealedUnits.h`,
`src/game/faction/DiplomacyActions.cpp`, `include/game/faction/DiplomacyActions.h`,
`src/game/faction/DiplomacyLedger.cpp`, `include/game/faction/DiplomacyLedger.h`,
`src/game/faction/DiplomaticActionExecutor.cpp`, `include/game/faction/DiplomaticActionExecutor.h`,
`src/game/faction/FirstContactResolver.cpp`, `include/game/faction/FirstContactResolver.h`,
`include/game/faction/FactionPair.h`, `src/game/faction/TradeItem.cpp`,
`include/game/faction/TradeItem.h`

**Assessment:** The two questions this slice is most often asked, it answers well. Unit
ownership has exactly one owner — `UnitManager::m_units` — with no leaked owning container
(`Units()` is a reference view) and no second position store: `Unit::m_pTile` is written
only by `UnitPositionIndex`, which is `friend`-gated. Diplomatic state cannot desync,
because `DiplomacyLedger` keys every symmetric fact on `FactionPair::Canonical` and every
asymmetric fact on `DirectedFactionPair`, so there is only one cell per fact. The three
visibility structures are likewise a clean split, not overlapping caches: explored
(monotonic memory), visible (derived, cleared and rebuilt), revealed-units (contact memory
keyed by stable id). The dominant weakness is *when* that derived state is recomputed — a
whole-map scan fires on every unit event — and the diplomacy executor, which validates a
proposal item-by-item but applies it as a whole, and reaches its collaborator through a
post-construction setter.

### [H] Visibility rebuild is a whole-map, per-event recompute
`src/game/faction/FactionVisibleMap.cpp:104` — `RebuildFromSources` clears and re-derives
the visible map by walking *every tile of the world*, and for each improvement on each tile
calling `SightRadiusFromImprovement_` (`FactionVisibleMap.cpp:32`), which allocates a
`std::vector<ActiveEffect_t>` and runs `ResolveStatModifiers` per candidate effect. That
result depends only on static `ImprovementConfig_t` data, so all of it is recomputed for
nothing. The trigger frequency makes it worse: `UnitManager::CreateUnit` (`UnitManager.cpp:66`)
and `DestroyUnit` (`UnitManager.cpp:117`) each rebuild, and `DestroyUnit` recurses per
passenger, so sinking a loaded transport does one full-map scan per unit aboard; every move
rebuilds via `GameState`'s `OnUnitMoved`; and each rebuild then invokes
`FirstContactResolver::ConsiderObserver` (`FirstContactResolver.cpp:62`), which scans every
other faction's units and bases. Memoize the per-config sight radius and keep a
vision-improvement tile list on the world (or gate the rebuild behind a dirty flag /
deferral scope like `DeferredDestructionScope`) before map or unit counts grow.

### [H] `DestroyUnit` is also the transfer path, so transfers apply combat cargo rules
`src/game/faction/UnitManager.cpp:77` — `DestroyUnit` implements "carrier is lost": cargo
that cannot hold the tile alone is destroyed, the rest is disembarked. `Faction::ExtractUnit`
(`src/game/Faction.cpp:357`) calls the same method to detach a unit for transfer, so
`TransferUnitTo` on a loaded transport silently drowns or strands its passengers, and the
receiving faction gets an empty hull. The reverse case is worse: extracting an *embarked*
unit clears its carrier link, and the re-create then runs
`CanPlaceUnitOnTile` (`UnitManager.cpp:54`) against a tile the carrier still occupies —
under `SetSingleUnitPerTile(true)` that throws *after* the source unit is gone, losing it.
`UnitManager` needs a "release ownership without applying destruction rules" operation
distinct from `DestroyUnit`.

### [H] Trade items are validated one at a time and applied all at once
`src/game/faction/DiplomaticActionExecutor.cpp:227` — each `TradeCredits_t` is checked
against the giver's *full* treasury independently, then `ApplyItems_` debits them all
(`DiplomaticActionExecutor.cpp:314`). Two credit items of the whole balance both validate
and both apply; `EconomyManager::AddEnergy` (`src/game/faction/EconomyManager.cpp:12`) has
no floor, so the giver ends the trade with a negative treasury and no error anywhere. The
same shape will hit every future consumable item type. Validate a proposal's aggregate cost
per giver, or debit through a checked operation that throws when it would go negative.

### [M] The executor is two-phase-initialised and its dependency is a nullable pointer
`include/game/faction/DiplomaticActionExecutor.h:27` — `SetGameDataContext` is a
post-construction setter for a hard dependency; `ApplyItem_` throws
"GameDataContext not set" mid-apply (`DiplomaticActionExecutor.cpp:323`) *after* earlier
items in the same proposal have already been applied, leaving a half-executed trade. This
is a fresh instance of prior finding 4.2 (recorded as fixed for every case it listed):
take `const GameDataContext&` in the constructor and hold it as a reference.

### [M] One global pending-proposal slot, silently overwritten
`src/game/faction/DiplomaticActionExecutor.cpp:105` — `Propose` to a player-controlled
faction assigns `m_pending` unconditionally. A second proposal in the same turn discards
the first, yet its proposer already received `PendingPlayer` and will wait forever. `Accept`
also does not identify who is accepting. A per-recipient queue (or at minimum rejecting a
proposal while one is pending) is needed before any AI diplomacy drives this.

### [M] `IsUnitVisibleTo` re-collects tile area effects once per concealment channel
`src/game/faction/UnitVisibility.cpp:45` and `:64` — `CollectConcealmentChannels_` collects
the subject tile's area effects, then `HasDetectionCovering_` collects *the same tile's*
area effects again for every channel found. `TileEffectsContext::CollectAreaEffects` returns
a freshly allocated vector and scans neighbouring tiles and units each call. This function
is the per-frame UI draw and pick gate (`src/ui/world/WorldView.cpp:258`,
`src/ui/world/UnitMarkerRenderer.cpp:52`) and is called O(tiles × units) from
`UnitOrderExecutor::CollectVisibleHostileIds_`. Collect once in `IsUnitVisibleTo` and pass
the result down.

### [M] A `Detect` effect with no `ownerFaction` reveals concealed units to *every* faction
`src/game/faction/UnitVisibility.cpp:22` — `AppliesForFaction_` treats an absent
`ownerFaction` as "applies to all observers". Only territory-owned improvements get
`ownerFaction` populated (`src/game/effects/TileEffectsContext.cpp:124`), so today this is
correct by accident: `Sensor` is the only `Detect` source in config and it is
`owned_by_territory`. The first unit-component or base `Detect` — exactly the kind of thing
a modder adds — will strip cloak for all factions at once, silently. Either require an owner
on `Detect` effects or resolve the owner from the effect's source unit/base.

### [M] Unsized visible map silently means "sees everything"
`src/game/faction/UnitVisibility.cpp:97` — `if (rObserver.GetVisibleMap().IsSized() && !...IsVisible(rTile))`.
A faction whose `BindWorldMap` was never called sees every enemy unit on the map instead of
failing. `Faction::RebuildVisibility` has the mirrored silent no-op on a null world map.
This is a wiring error masked as a game rule; it should throw, with fixtures binding a map.

### [M] `TradeKind` and its probe table duplicate the `TradeItem_t` variant three times
`include/game/faction/DiplomacyActions.h:23` and `src/game/faction/DiplomacyActions.cpp:106`
— adding an alternative to `TradeItem_t` requires editing the enum, the `probes` array, the
parallel `kindOrder` array, and `ToString`, and nothing fails to compile if you miss one.
The probe table also depends on an unenforced invariant stated only in a comment ("CanTrade
only gates on relationship") — it passes `TradeCommFrequency_t{0}` and `TradeBase_t{1}`,
which become wrong answers the moment `CanTrade` inspects a payload. Derive the category
list from the variant (`std::variant_size` + a per-alternative `kind` trait) instead.

### [M] `Military` leaks its owning container and swallows a null design
`include/game/faction/Military.h:18` — `GetDesigns()` returns
`const std::vector<std::unique_ptr<UnitDesign>>&`, the exact pattern the prior review closed
for `UnitManager::GetUnits()` ("last leaked owning container", finding 1.1). `DesignListPanel`
consumes it and must dereference the smart pointers itself. `AddDesign`
(`src/game/faction/Military.cpp:12`) also returns `false` for both "duplicate id" and "null
pointer", so callers that report "duplicate" (`src/game/Engine.cpp:232`,
`src/ui/unit-designer/UnitDesignerView.cpp:160`) will misreport a null. Expose a `DerefView`
range and throw on null per the guidelines.

### [M] `FirstContactResolver` depends on GameState's concrete ownership container
`include/game/faction/FirstContactResolver.h:19` — it stores
`std::vector<std::unique_ptr<Faction>>&`, which is why the implementation carries `if
(!pOther)` / `if (!pObserver)` guards (`FirstContactResolver.cpp:68`, `:91`) for a condition
`GameState::AddFaction` already rejects. Everywhere else this container is hidden behind
`GameState::Factions()` (a `DerefView`); taking that range here removes the null branches
and the coupling to how factions happen to be stored.

### [L] Convention and hygiene items
- Enum classes missing the mandatory `_t` suffix: `DiplomaticStatus`
  (`include/game/faction/DiplomacyLedger.h:14`), `DiplomaticActionKind` and `TradeKind`
  (`include/game/faction/DiplomacyActions.h:12`, `:23`), `DiplomaticProposeResult`
  (`include/game/faction/DiplomaticActionExecutor.h:12`).
- `ToString(DiplomaticStatus)` (`src/game/faction/DiplomacyLedger.cpp:6`) is a hand-rolled
  switch whose strings match the enumerator names exactly — `magic_enum` per the guidelines,
  with only the `None → ""` case special.
- `DiplomaticActionExecutor.cpp:20-42` reimplements `FindFaction_` (both const overloads)
  when `GameState::FindFaction` (`include/game/GameState.h:81-82`) already exists.
- `AppliesForFaction_` is duplicated verbatim in `src/game/faction/UnitVisibility.cpp:22` and
  `src/game/effects/TileEffectsContext.cpp:167`.
- `std::map<FactionPair, bool> m_known` and `std::map<DirectedFactionPair, bool>
  m_infiltration` (`include/game/faction/DiplomacyLedger.h:59-61`) never store `false` — the
  setters erase instead — so the mapped value is dead weight; `std::set` states the invariant.
- Dead / speculative API in `include/game/faction/FactionRevealedUnits.h`: `Clear()` (:20),
  `GetRevision()` (:52), and the `UnitId_t` overloads of `Reveal`/`Forget`/`IsRevealed` have
  no callers anywhere, production or test. Same for `FactionVisibleMap::IsRemoveFog()`,
  `GetRevision()`, `GetWidth()`, `GetHeight()` (`FactionVisibleMap.h:30-35`, `:49`).
- `GetAvailableActions` / `GetAvailableTrades` / both `ToString` overloads in
  `DiplomacyActions.h:47-57` have no production caller — only tests.
- `src/game/faction/VisibilityRules.cpp:35` calls `MarkAll()` (a W×H write) after
  `SetRemoveFog(true)`, but `FactionVisibleMap::IsVisible` already short-circuits on the
  flag; the fill is redundant and runs on every rebuild.
- `FirstContactResolver::MeetIfNeeded_` (`FirstContactResolver.cpp:16`) re-checks `AreKnown`
  that both callers already checked; `ConsiderObserver` takes `Faction&` but never mutates it.
- `src/game/faction/TradeItem.cpp:3` includes `<sstream>` and never uses it;
  `include/game/faction/Military.h:19` uses `std::string` without including `<string>`.

**Observed outside slice:**
- `src/game/units/MovementRules.cpp:142` — `CanPlaceUnitOnTile` counts embarked cargo as
  occupying the tile, while `StepEvaluator.cpp:233` explicitly skips embarked units under
  the same stacking rule; the two readings of one rule disagree.
- `src/game/map/TileFlagMap.h:39` — `MergeFrom` is a silent no-op when sizes differ, so a
  `TradeWorldMap_t` between factions with mismatched maps would appear to succeed.
