## UI — manager, views, tile rendering

**Files:** `src/ui/PlaceholderPanel.cpp`, `include/ui/PlaceholderPanel.h`, `src/ui/TileHitTester.cpp`, `include/ui/TileHitTester.h`, `src/ui/TileRenderer.cpp`, `include/ui/TileRenderer.h`, `src/ui/UIManager.cpp`, `include/ui/UIManager.h`, `src/ui/ViewFactory.cpp`, `include/ui/ViewFactory.h`, `include/ui/IGameView.h`, `include/ui/UIElement.h`

**Assessment:** `UIManager` / `IGameView` / `UIElement` form a small, readable stack with clear ownership (`unique_ptr` views and elements). `TileRenderer` correctly pulls colors from `UiStyle`, and `PlaceholderPanel` is an honest stub. The dominant weaknesses are input/stack policy gaps in `UIManager`, null-tolerant factory returns that fight project guidelines, and a stale `TileHitTester` workable-area API that no longer matches its callers.

### [H] Block global shortcuts from stacking overlays
`src/ui/UIManager.cpp:42-72` — `ProcessKeys_` falls through to `HandleGlobalShortcut_` whenever the active view returns `false` from `HandleKey`. Overlay views only consume `Escape` (e.g. research/settings/base), so repeated `F2`/`E`/`U`/… pushes another full-screen view each time. `CombatView` already documents this hazard by swallowing every key (`src/ui/world/CombatView.cpp:77-81`); the fix belongs in `UIManager` (ignore shortcuts while an overlay is active, or replace/dedupe by kind) rather than in each view.

### [H] Do not return null views or push unchecked pointers
`src/ui/ViewFactory.cpp:63-67,77-80,90-93,108-111` — several `Create*View` methods return `nullptr` when `GetPlayerFaction()` is missing, against “prefer throwing / throw on unexpected null.” `UIManager::PushView` (`src/ui/UIManager.cpp:109-112`) unconditionally calls `pView->OnPushed` with no null check. Shortcut factories null-check before push (`UIManager.cpp:68-71`), but open-base and similar callers can pass a null `unique_ptr` straight in. Throw from the factory (and/or `PushView`) so missing player state fails loudly.

### [M] Stop culling closed views/elements inside Render
`include/ui/IGameView.h:21-28` and `src/ui/UIManager.cpp:94-101` — element and overlay lifetime is mutated during `Render` (`ShouldClose` → erase / `OnPopped`). Until the next render, a closed overlay remains `GetActiveView_()` and still receives further mouse events drained in the same `ProcessMouse_` loop (`UIManager.cpp:74-83`). Prior review 4.3 deferred renaming `IGameView` and moving prune out of `Render`; the incomplete fix still leaves input routed at a dying top view. Prune (or pop) closed views at the start/end of `ProcessInput`, not inside paint.

### [M] Remove or rewire dead workable-area hit test
`include/ui/TileHitTester.h:10,23-36` / `src/ui/TileHitTester.cpp:49-81` — `HitTestBaseWorkableArea` has no callers; `BaseWorkableAreaDisplay` hit-tests its own cached rects. The header still claims shared use with that display. The helper hardcodes `k_WorkableGridRadius = 2` separately from `MapUtils`’s `k_WorkableRadius`, excludes the origin, and returns unwrapped `baseX+dx` (model iteration wraps via `WorldMap::GetTile`). Delete it, or implement one shared path that calls `InEuclideanRadius` / workable helpers so UI and rules cannot drift.

### [M] Clarify HitTestWorldGrid contract
`include/ui/TileHitTester.h:14-21` — comments say “full world map” and “world tile coordinates,” but the function returns origin-relative grid indices for whatever width/height the caller passes. `WorldView` correctly feeds the visible viewport size and then converts via `WorldCoordsAt`. Rename parameters/docs (e.g. `gridCols`/`gridRows`, “grid indices”) so the next caller does not treat the result as world coordinates.

### [M] Stop hardcoding the Forest improvement id in TileRenderer
`src/ui/TileRenderer.cpp:102-104` — fill color keys off `HasImprovement("Forest")`. Forest is a config id (`config/improvements.json`); a mod rename or alternate forest-like improvement silently loses the overlay color. Drive the special-case id (or a style flag) from config/`UiStyle`, same as the other tile colors.

### [M] Narrow ViewFactory’s header dependencies
`include/ui/ViewFactory.h:5-14` — the factory header includes every concrete view type. Any TU that needs `ViewFactory` recompiles when any view changes, and the factory surface depends on concretions rather than `IGameView`. Forward-declare return types in the header (or return `unique_ptr<IGameView>` where practical) and include concrete views only in `ViewFactory.cpp`.

### [L] Convention and hygiene items
- `include/ui/IGameView.h:13` — concrete base with state named like an interface; prior 4.3 deferred rename to `GameView`.
- `include/ui/UIElement.h:61-64` — `Contains` duplicates `ContainsMouseCoord` on the same `Rectangle_t` shape.
- `include/ui/UIElement.h:37-38` — `ResolveLayout` rejects ratios `> 1` but allows negatives.
- `include/ui/UIManager.h:17,44` — uses `std::function` / `std::unordered_map` without including `<functional>` / `<unordered_map>`; `ViewFactory_t` aliases collide conceptually with class `ViewFactory`.
- `src/ui/TileHitTester.cpp:74-80` — `IsInWorkableDiamond_` names a Euclidean disk; `k_BaseCenterOffset = 0` is noise for “exclude origin.”
- `src/ui/TileRenderer.cpp:25-50` — `MoistureToInt_` / `RockinessToInt_` use `default:` → `0` instead of throwing on unexpected enumerators.
- `src/ui/ViewFactory.cpp:83` — `ResearchView` still takes `const ResearchManager*` rather than a reference.

**Observed outside slice:**
- `src/game/Engine.cpp:418-468` — shortcuts capture `GetFullscreenLayout()` once at init (prior 4.5 resize/layout drift still open); open-base lambda pushes `CreateBaseView` with no null check.
- `docs/architecture/ui-system.md` — still describes `UIManagerImpl`, `UIWorldMap`/`UIPanel`/`UIPopup`, and `IGameView::Update`; does not match this slice.
- `src/ui/base/BaseWorkableAreaDisplay.cpp:106-136` — owns hit-testing that made `HitTestBaseWorkableArea` dead (fix lives there if the shared helper is restored).
