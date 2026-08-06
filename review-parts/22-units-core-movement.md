## Units — model, orders, movement

**Files:** `FoundBaseRules.{h,cpp}`, `MoveCostCalculator.{h,cpp}`, `MovementRules.{h,cpp}`, `Pathfinder.{h,cpp}`, `StepEvaluator.{h,cpp}`, `TerraformRules.{h,cpp}`, `TransportRules.{h,cpp}`, `Unit.{h,cpp}`, `UnitComponentConfig.h`, `UnitComponentConfigParser.{h,cpp}`, `UnitComponentRegistry.h`, `UnitDesign.{h,cpp}`, `UnitOrder.{h,cpp}`, `UnitOrderExecutor.{h,cpp}`, `UnitSlotConfig.h`, `UnitSlotConfigParser.{h,cpp}`, `UnitSlotRegistry.h`, `UnitDomain.h`, `IUnitOrderWorld.h`, `MovementConstants.h`

**Assessment:** Movement is well factored along the architecture doc: `MoveCostCalculator` owns entry terms, `StepEvaluator` owns legality (objective vs faction-known), `Pathfinder` plans, `UnitOrderExecutor` mutates. Transport boarding vs unaided occupation is clear and tested. The dominant weaknesses are execution-path cost (full-map hostile scans and full Dijkstra per step) and a few silent failure / two-phase-init traps that will bite as conquest and terraform see more use.

### [H] Stop scanning the whole map for hostiles on every step
`src/game/units/UnitOrderExecutor.cpp:73-94` — `CollectVisibleHostileIds_` walks every tile and every unit, calling `IsUnitVisibleTo` each time. `TryStep` (`:218-219`, `:99-100`) does this twice per attempted step (before + after). A multi-step `Execute_` move therefore pays O(steps × tiles × units × visibility work) even in empty fog. On a real map this will dominate the PlayerActions pass. Restrict the scan to tiles whose visibility changed this step (or maintain a faction-visible-hostile set invalidated by reveal / move).

### [H] Stacking left the position index; the process-global is an incomplete prior fix
`src/game/units/MovementRules.cpp:21-42,130-143` — prior review 2.2 recorded stacking as enforced inside `UnitPositionIndex::TryMoveUnit` / `SetSingleUnitPerTile`. That API is gone: `MoveUnit` no longer checks capacity, and the only switch is file-scope `s_bSingleUnitPerTile` consulted by `CanPlaceUnitOnTile` / `StepEvaluator`. Any caller that moves without the step check can overstack, and tests can leak the global across cases. Put the rule on world/game config owned beside the index, and enforce it at the same mutation boundary that updates occupancy (or make `MoveUnit` reject illegal stacks).

### [M] Terraform completion ignores apply failure after energy was spent
`src/game/units/UnitOrderExecutor.cpp:504-518` — after counting down `TerraformOrder_t`, `ApplyTerraformResult`’s `bool` is discarded and the order always `Complete`s. Energy was already debited in `TryStartTerraform` (`:363-366`). If another former changes the tile mid-project, or the improvement id disappears from the registry (`:506-509` also completes with no apply), the player loses turns and energy with no mutation and no refund. Surface failure (keep/retry/refund) instead of treating a failed apply as success.

### [M] Conquest depends on a post-ctor nullable `GameDataContext`
`include/game/units/UnitOrderExecutor.h:57-59` / `src/game/units/UnitOrderExecutor.cpp:155-160,296-303` — `m_pGameData` is set only via `SetGameDataContext`. With `m_pWorld` bound but data unset, `ApplyArrivalEffects_` and post-combat conquest silently no-op (no capture, no raid). Same two-phase pattern as `DiplomaticActionExecutor` (prior 4.2 / slice 09). Take `const GameDataContext&` in the constructor when a world is supplied, or fail loudly if conquest is invoked unbound.

### [M] `NextStep` always runs a full Dijkstra
`src/game/units/Pathfinder.cpp:144-151` / `UnitOrderExecutor.cpp:414-415` — re-planning after each step is required (fog/contact), but `NextStep` materializes the entire path (`FindPath`) only to return `tiles.front()`. Every move fragment spent pays a full O(tiles log tiles) search plus two `tileCount`-sized vectors. Early-exit Dijkstra (stop when the first step off the origin is finalized) or a search that returns only the successor would keep the semantics at a fraction of the cost.

### [M] Sea-former domain rules hardcode improvement ids
`src/game/units/TerraformRules.cpp:47-49` — `KelpFarm` / `MiningPlatform` / `TidalHarness` are special-cased by id, while `config/improvements.json` already tags them `sea_terraform`. A modded sea improvement with the tag but a new id is treated as land-only (or wrongly allowed). Drive `DomainAllows_` from tags / config fields, not a closed id list.

### [M] `EmbarkInto` does not enforce carrier invariants
`src/game/units/Unit.cpp:128-140` — public `EmbarkInto` links cargo with no same-tile, capacity, domain, or faction checks (`TransportRules` documents those as caller duties). A single missed call site overfills `m_cargo` or embarks across tiles; `FreeCargoSlots` then goes negative and `MoveUnit` will still tow the passenger. Enforce the predicates inside `EmbarkInto` (or make it private to `TransportRules`).

### [L] Convention and hygiene items
- `src/game/units/MoveCostCalculator.cpp:25` — `k_RoadId = "Road"` couples fungus-as-road to a magic improvement id; prefer a config/tag look-up.
- `src/game/units/StepEvaluator.cpp:26` — embarked-in-base test uses `HasImprovement("Base")` string rather than the founding/base-tile predicate used elsewhere.
- `src/game/units/UnitOrderExecutor.cpp:393-394`, `MoveCostCalculator.cpp:118-119` — single-line `if` bodies omit braces required by project style.
- `src/game/units/TerraformRules.cpp:80-91,216-244` — raise/lower elevation bands (`1000` / `3500`) are magic numbers with no named constants or config.
- `src/game/units/UnitComponentConfigParser.cpp:14-23` — combat-rating target map is fine (wire form ≠ enumerator), but sits far from `CombatRatingTarget_t`; keep the map next to the enum per guidelines.
- `include/game/units/UnitOrderExecutor.h:25` — forward-declares unused `GameState` (only needed in the `.cpp` / method signatures that already include it).

**Observed outside slice:**
- `src/game/units/AttackRules.cpp:78` — declare-attack allows any positive fragment balance while `TryAttack` always deducts a full move point (clamped), so a 1-fragment unit can still attack.
- `src/game/map/UnitPositionIndex.cpp:17-36` — `MoveUnit` trusts callers for stacking; pairs with the incomplete 2.2 fix above.
- `src/game/faction/UnitVisibility.cpp` — `IsUnitVisibleTo` re-collects tile area effects per channel; amplifies `CollectVisibleHostileIds_` (already noted in slice 09).
