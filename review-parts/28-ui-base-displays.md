## UI — base view and displays

**Files:** `src/ui/base/BaseNameDisplay.cpp`, `include/ui/base/BaseNameDisplay.h`, `src/ui/base/BaseView.cpp`, `include/ui/base/BaseView.h`, `src/ui/base/BaseWorkableAreaDisplay.cpp`, `include/ui/base/BaseWorkableAreaDisplay.h`, `src/ui/base/GrowthDisplay.cpp`, `include/ui/base/GrowthDisplay.h`, `src/ui/base/PopulationDisplay.cpp`, `include/ui/base/PopulationDisplay.h`, `src/ui/base/ProductionDisplay.cpp`, `include/ui/base/ProductionDisplay.h`, `src/ui/base/SupportDisplay.cpp`, `include/ui/base/SupportDisplay.h`

**Assessment:** The slice is a thin coordinator (`BaseView`) plus focused read-only panels that pull layout/colors from `UiStyle` and stay easy to scan. The dominant weaknesses are interaction design (non-modal in-view popups), constructors that accept nullable pointers the caller always has as references, and presentation rules (pop glyphs) hard-coded in render code instead of config.

### [M] Make in-view selector popups modal
`src/ui/base/BaseView.cpp:206-212` / `225-238` — `HandlePopClick` / `HandleProductionDisplayClicked_` push `PopTypeSelectorPopup` / `ProductionSelectorPopup` into `m_elements`, but `BaseView` does not override `HandleMouse`. `IGameView::HandleMouse` delivers the click only to the topmost element whose `Contains` is true; both popups use partial layouts (`popupSmall` / `topPanel`), so clicks on tiles, pops, or production still reach the underlying panels, can mutate assignments, and can stack a second popup. Fix: override `HandleMouse`/`HandleKey` to consume input while any popup child is open (or insert a fullscreen dismiss layer), and dismiss/replace an existing selector before opening another.

### [M] Drop the separate `Faction&`; use the base’s owner
`include/ui/base/BaseView.h:24` / `43`, `src/ui/base/BaseView.cpp:207` — `BaseView` stores `m_rFaction` only for `GetAvailablePopTypes()`, while `BaseManager::GetFaction()` already exposes the owning faction. `ViewFactory` always passes the player faction even when `bEditable` is false for a foreign base, so the view carries a faction reference that can disagree with `m_rBase`. Fix: remove the `Faction` constructor parameter and call `m_rBase.GetFaction().GetAvailablePopTypes()`.

### [M] Require non-null base/population at construction (use references)
`include/ui/base/GrowthDisplay.h:15`, `ProductionDisplay.h:16`, `BaseNameDisplay.h:15`, `SupportDisplay.h:15`, `BaseWorkableAreaDisplay.h:25`, `PopulationDisplay.h:20` — every panel takes a nullable `const BaseManager*` / `PopulationManager*` though `BaseView` always passes a live object. Most panels only throw on null inside `Render` (`GrowthDisplay.cpp:22-25`, etc.), so a null digs in as a latent invalid object. Guidelines require valid construction and prefer references. Fix: take `const BaseManager&` / `PopulationManager&` (throw in the ctor if a pointer API must remain). Related to prior review §6 pointer→reference cleanup (still open).

### [M] Hard-coded pop display glyphs belong in config
`src/ui/base/PopulationDisplay.cpp:111-122` — letter labels use the first character of the type id, then special-case `IsDrone()` → `'R'` and `IsTalent()` → `'A'` to dodge Drone/Doctor and Talent/Technician collisions. Any new modded type that shares an initial (or a non-drone with `riot_contribution > 0`) silently gets a wrong or colliding glyph; `PopTypeConfig_t` has no display-letter field. Fix: add an explicit glyph (or icon id) on pop-type config and render that.

### [M] Panels still force full live yield/production work every frame
`src/ui/base/BaseWorkableAreaDisplay.cpp:71-72` / `87-89`, `GrowthDisplay.cpp:46-51`, `ProductionDisplay.cpp:56-66` — each frame re-queries `GetWorkedTileYield` / `GetPreviewTileYield` per workable tile and nutrient/mineral production getters. Pool filtering is memoized (prior §1.1 fixed), but `ResourceManager::ComputeWorked_` memoization and `CollectAreaEffects` cost remain deferred there; these UI call sites are still the hot drivers with no display-side revision stamp. Fix: render from a per-frame or revision-keyed snapshot (or skip re-query when worker/effects revisions are unchanged).

### [M] Player actions mutate managers with no command/event seam
`src/ui/base/BaseView.cpp:176-196`, `222`, `237` — tile assign/unassign, reset-all, `ConvertPop`, and `SetProduction` are direct manager calls from UI handlers. Mods cannot observe or intercept these actions (prior §1.10, still open). Fix direction lives partly here: route through a command/action API that emits events before touching `BaseManager`.

### [L] Convention and hygiene items
- `include/ui/base/BaseView.h:37-38` — `HandlePopClick` / `HandlePopTypeSelected` are private but lack the trailing `_` used by `HandleTileClick_` / `HandleBaseClicked_` (prior §6 called this out; still true).
- `include/ui/base/BaseWorkableAreaDisplay.h:14` — comment says “21 tiles”; workable set is the 20-tile ring and the center is drawn separately (`BaseWorkableAreaDisplay.cpp:75-78`; see `MapUtils.h` radius-2 note).
- `include/ui/base/PopulationDisplay.h:3` — includes `ui/IGameView.h` though the class only needs `UIElement` (and `<vector>` for `m_popBoxes` is missing, relying on transitive includes).
- `src/ui/base/GrowthDisplay.cpp` / `ProductionDisplay.cpp` — nearly identical panel chrome/line layout; a tiny shared helper would remove drift risk.
- `src/ui/base/SupportDisplay.cpp:52-55` — units that do not fit are dropped with `break` and no overflow cue.
- No tests under `tests/` exercise these displays or `BaseView` input (pop convert, tile assign, production click, editable gating).

**Observed outside slice:**
- `docs/architecture/ui-system.md` / `high-level.md` still describe `IBasePanel` / `BaseDisplay`; neither exists — live types are `UIElement` subclasses including `BaseNameDisplay`, `ProductionDisplay`, `SupportDisplay`.
- `src/ui/base/ProductionSelectorPopup.cpp:92-96` — outside-click dismiss never runs: `IGameView::HandleMouse` only invokes `HandleMouseClick` when `Contains` is true.
- `src/ui/ViewFactory.cpp:69-70` — always passes the player `Faction` into `BaseView` (feeds the dual-faction issue above).
