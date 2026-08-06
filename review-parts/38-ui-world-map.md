## UI — world map and combat presentation

**Files:** `src/ui/world/CameraInputController.cpp`, `include/ui/world/CameraInputController.h`, `src/ui/world/CombatPresentation.cpp`, `include/ui/world/CombatPresentation.h`, `src/ui/world/CombatView.cpp`, `include/ui/world/CombatView.h`, `src/ui/world/MapViewport.cpp`, `include/ui/world/MapViewport.h`, `src/ui/world/MinimapDisplay.cpp`, `include/ui/world/MinimapDisplay.h`, `src/ui/world/UnitMarkerRenderer.cpp`, `include/ui/world/UnitMarkerRenderer.h`, `src/ui/world/UnitOrderInputController.cpp`, `include/ui/world/UnitOrderInputController.h`, `src/ui/world/WorldDisplay.cpp`, `include/ui/world/WorldDisplay.h`, `src/ui/world/WorldView.cpp`, `include/ui/world/WorldView.h`

**Assessment:** `MapViewport` cleanly owns cylindrical wrap math, and the split between `WorldDisplay` / `UnitMarkerRenderer` / input controllers is mostly coherent. Dominant weaknesses are modal lifetime and turn-gating in `WorldView` (in-view popups are not real overlays), plus combat playback that reads live map state after `Resolve` has already mutated positions. `CameraInputController` and `MinimapDisplay` are in good shape.

### [H] Gate turns and map input while WorldView modal popups are open
`src/ui/world/WorldView.cpp:320-335`, `src/ui/world/WorldView.cpp:541-571`, `src/ui/world/WorldView.cpp:387-390`, `src/ui/world/WorldView.cpp:273-278`, `src/ui/world/WorldView.cpp:404-418` — `SupplyCrawlPopup` / `ProbeActionPopup` are pushed onto `m_elements` and capture raw `Unit*`, but unlike `BaseView` they are not overlay views. `Enter`, the End Turn button, and auto-advance in `Update_` can still call `m_onProcessTurn()`; `OnUnitDestroyed` clears `m_pSelectedUnit` but the lambda still compares/uses a dangling `pUnit`/`pProbe`. Chrome hit-testing only forwards presses to elements that `Contains` the cursor, so the popups' click-outside dismiss path never runs and map selection continues underneath. This reopens the class of defect marked fixed in review 1.8 for overlay popups — treat these as modal (block turn + route all input to the popup, or push a real overlay).

### [H] Do not open BaseView when a visible unit was selected on the tile
`src/ui/world/WorldView.cpp:494-503` — after `SelectUnitAtTile_`, the handler always calls `m_onOpenBase` when `FindBaseAt` hits. Architecture (`docs/architecture/ui-system.md`) requires base-open only when the tile has no (visible) units. As written, every garrison click steals focus into BaseView, and `FindBaseAt` can return any faction's base. Open the base only when selection cleared to no unit (or via an explicit base UI affordance).

### [M] Prefer fight-tile hit placement for the whole combat playback
`src/ui/world/CombatPresentation.cpp:103-132` — `Render` prefers the live `UnitMarkerRenderer` cache and only ghosts onto `m_pAttackerTile` / `m_pDefenderTile` when `WasDestroyed_`. `CombatResult_t` documents that Resolve already applied retreat/`DestroyUnit` before playback; a disengaged survivor is drawn at `pRetreatTile`, so mid-replay hit flashes appear on the retreat tile, not the fight tile. While `IsActive()`, place overlays from the recorded combatant tiles (cache only as a same-tile refinement), matching the “replay history” contract.

### [M] Do not run ProcessTurn from inside Render
`src/ui/world/WorldView.cpp:125-132`, `src/ui/world/WorldView.cpp:273-278` — with pause-at-end off, `Update_()` (called from `Render`) invokes `m_onProcessTurn()` when the last unit stops needing orders. That mutates the full turn pipeline mid-frame after the world has already been drawn and before overlays render — reentrancy and half-updated UI. Queue the auto-end-turn for the input/update phase (or a post-render callback), not the draw path.

### [M] Resolve defender display name with the same unit Resolve attacked
`src/ui/world/WorldView.cpp:574-584`, `src/ui/world/WorldView.cpp:511-512` — `FindUnitNameOnTile_` returns the first non-null unit on the tile. Stacked units make the CombatView defender label wrong relative to `TryAttack` / `FindAttackableHostileOnTile`. Capture the defender name from the unit actually fought (or the attackable hostile lookup) before Resolve.

### [M] Stop hardcoding improvement IDs in the map renderer
`src/ui/world/WorldDisplay.cpp:154`, `src/ui/world/WorldDisplay.cpp:186` — Sensor/Monolith markers key on string literals `"Sensor"` / `"Monolith"`. Markers silently vanish if config ids change; drive marker kinds from improvement config (or a style/config map of id → marker), not compile-time strings.

### [L] Convention and hygiene items
- `src/ui/world/WorldDisplay.cpp:27-41` / `src/ui/world/MinimapDisplay.cpp:23-37` — duplicated `PlayerFogMaps_t` / `PlayerFog_` helpers with divergent member names (`explored` vs `pExplored`).
- `src/ui/world/CameraInputController.cpp:113-126` — uses `s.relativeMin` as the “no scroll” sentinel; works only while `relative_min` stays `0.0` in style JSON — use an explicit zero / optional direction instead.
- `src/ui/world/UnitMarkerRenderer.cpp:48-51` — unexpected null `Unit*` in the tile stack is skipped; project rules prefer throw on unexpected null.
- `src/ui/world/UnitOrderInputController.h:29-30` — nullable `GameState*` / `GameDataContext*` default to silent no-op for attack/probe; call sites always pass them — prefer references (or assert) once the deferred path is gone.
- `include/ui/world/UnitMarkerRenderer.h` / `WorldDisplay` — almost no automated coverage for wrap hit-testing, hold-to-move, or combat playback timing despite substantial branching.

**Observed outside slice:**
- `src/ui/UIManager.cpp:86-105` — overlay `ShouldClose` is checked before `CombatView::Render` runs `Update`, so close lags one frame (acceptable, but the only place playback advances).
- `docs/architecture/ui-system.md` — still describes right-click hold for move orders; implementation is left-click hold in `UnitOrderInputController`.
