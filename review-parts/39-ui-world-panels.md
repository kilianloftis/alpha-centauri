## UI — world panels and order input

**Files:** `src/ui/world/CommlinksButton.cpp`, `include/ui/world/CommlinksButton.h`, `src/ui/world/EndTurnButton.cpp`, `include/ui/world/EndTurnButton.h`, `src/ui/world/InfoPanelElement.cpp`, `include/ui/world/InfoPanelElement.h`, `src/ui/world/LocationPanel.cpp`, `include/ui/world/LocationPanel.h`, `src/ui/world/ProbeActionPopup.cpp`, `include/ui/world/ProbeActionPopup.h`, `src/ui/world/SelectedUnitPanel.cpp`, `include/ui/world/SelectedUnitPanel.h`, `src/ui/world/SupplyCrawlPopup.cpp`, `include/ui/world/SupplyCrawlPopup.h`, `src/ui/world/TerraformInputController.cpp`, `include/ui/world/TerraformInputController.h`, `src/ui/world/UnitStackPanel.cpp`, `include/ui/world/UnitStackPanel.h`

**Assessment:** Dashboard panels (`SelectedUnitPanel`, `LocationPanel`, `InfoPanelElement`, `UnitStackPanel`) and the two chrome buttons are small, style-driven, and easy to follow. The weak spots are order-entry surfaces: terraform hotkeys hardcode every improvement id in C++, and `ProbeActionPopup` / `SupplyCrawlPopup` are forked copies of `ProductionSelectorPopup` with the same unreachable outside-dismiss and soft callback guards. Nothing in this slice is large; the risk is silent key/config drift and modal UX that looks finished but is not.

### [H] Load terraform key→improvement bindings from config
`include/ui/world/TerraformInputController.h:31-54` hardcodes twenty-two `Key_t` → improvement id strings (`"Road"`, `"Farm"`, `"SolarCollector"`, …) that must stay byte-identical to `config/improvements.json`. Renames, new Former projects, or modded hotkeys require a C++ edit; a mismatch only fails later inside `TryStartTerraform` with no construction-time check. Drive the map from config (or improvement metadata) and validate ids against the registry when the controller is built.

### [H] Stop cloning ProductionSelector for probe and supply popups
`src/ui/world/ProbeActionPopup.cpp` and `src/ui/world/SupplyCrawlPopup.cpp` duplicate the ProductionSelector layout/render/Escape/click path (same `Style().productionSelectorPopup`, same `CacheEntryRects_`, same dismiss shape), already called out for the base selectors in part 29. They will keep drifting (labels, empty-callback policy, overflow) independently. Collapse onto one typed list-selector helper so dismiss, hit-testing, and clipping have a single owner.

### [M] Outside-click dismiss in probe/supply popups is unreachable
`src/ui/world/ProbeActionPopup.cpp:84-88` and `src/ui/world/SupplyCrawlPopup.cpp:82-86` set `m_bShouldClose` when the click is outside `m_layout`, but `WorldView::HandleMouse` (and `IGameView::HandleMouse`) only invoke `HandleMouseClick` after `Contains` on that layout. The branch never runs; Escape is the only in-popup dismiss. Remove the dead branch until modal routing exists, or give the popup a fullscreen hit target with inset chrome.

### [M] UnitStackPanel silently drops units that do not fit
`src/ui/world/UnitStackPanel.cpp:49-57` stops laying out when `x + slotWidth > right`, with no scroll, wrap, or overflow cue. Stacks wider than the panel become partially invisible and unselectable even though `SetUnits` received them. Bound visibly and offer scroll/paging (or signal overflow) so tile/base stacks remain fully reachable.

### [M] Soft-skip empty callbacks instead of requiring a valid handler
`CommlinksButton.cpp:29-32`, `EndTurnButton.cpp:31-34`, `ProbeActionPopup.cpp:94-97`, `SupplyCrawlPopup.cpp:92-95`, and `UnitStackPanel.cpp:101-104` no-op when the `std::function` is empty. Probe/supply also leave the popup open on an empty handler after a row hit. Against “throw on unexpected null / construct only valid objects”: require a non-empty callback in the constructor (and dismiss the popup after a resolved row).

### [M] Hardcode supply-crawl resource choices in the popup
`src/ui/world/SupplyCrawlPopup.cpp:16-20` fixes Nutrients/Minerals/Energy labels and `StatId_t` values in C++. Crawlable stats belong in config (or a shared rules table) so a mod cannot extend the picker without editing this file. Build the entry list from data the order rules already trust.

### [L] Convention and hygiene items
- `include/ui/world/TerraformInputController.h:31-54` — `m_bindings` is a per-instance `const unordered_map` data member; prefer `static constexpr` / file-scope table once bindings leave hardcoding.
- `src/ui/world/ProbeActionPopup.cpp:45` / `SupplyCrawlPopup.cpp:48` — reuse `Style().productionSelectorPopup` with no dedicated style keys; production chrome tweaks restyle these menus.
- `src/ui/world/UnitStackPanel.cpp:51-54` — null `pUnit` uses `break` (truncates the rest of the row) instead of `continue`; callers currently filter nulls, so this is a latent trap.
- `include/ui/world/ProbeActionPopup.h:24` / `SupplyCrawlPopup.h:24` — redundant `~…() override = default`.
- `include/ui/world/InfoPanelElement.h:23` — `InfoLine` default color calls `Style()` in a default member initializer (fine only after `UiStyle::Load`).
- No tests under `tests/` cover terraform key mapping, probe/supply hit-testing/Escape, or unit-stack overflow/click selection.

**Observed outside slice:**
- `src/ui/world/WorldView.cpp:404-418` — while `SupplyCrawlPopup` / `ProbeActionPopup` are open, clicks outside their rects fall through to map/order handling (move preview, tile select) instead of dismissing or being swallowed; modal capture belongs in view mouse routing.
- `src/ui/world/WorldView.cpp:301-360` vs terraform `O`/`B`/`L` — order-controller keys overlap Former bindings; L has an intentional attach fall-through, but O/B still steal Forest/Bunker when the unit also has SupplyCrawl/FoundBase.
- `docs/architecture/ui-system.md` — still omits these world dashboard panels, terraform controller, and probe/supply popups.
