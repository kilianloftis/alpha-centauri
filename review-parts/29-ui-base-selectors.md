## UI — base selector popups

**Files:** `src/ui/base/PopTypeSelectorPopup.cpp`, `include/ui/base/PopTypeSelectorPopup.h`, `src/ui/base/ProductionSelectorPopup.cpp`, `include/ui/base/ProductionSelectorPopup.h`

**Assessment:** Both popups are small, readable list pickers that pull chrome from `UiStyle` and correctly dismiss on Escape. The dominant weakness is that they are near-copies of each other (and of several other selector popups) that have already diverged in dismiss and null handling, with no clipping/scroll for long lists and no real modal hit-testing. Nothing here is large or clever; the risk is silent UX breakage as constructable/pop-type lists grow.

### [H] Extract the duplicated list-selector instead of letting copies diverge
`src/ui/base/PopTypeSelectorPopup.cpp` and `src/ui/base/ProductionSelectorPopup.cpp` are the same widget (cache entry rects → draw header/list → Escape closes → click invokes callback) with different payload types and copy strings. They have already drifted: Production closes on “outside” click and soft-skips null entries (`ProductionSelectorPopup.cpp:92-106`); PopType neither dismisses outside nor null-checks before deref (`PopTypeSelectorPopup.cpp:95-112`). Further selector copies in the tree (`ComponentSelectorPopup`, `CouncilProposalsPopup`, `SupplyCrawlPopup`, …) show the same fork. Collapse to one typed/list-callback helper (or shared private base) so dismiss, null, and layout rules have a single owner.

### [M] Outside-click dismiss in ProductionSelector is unreachable
`src/ui/base/ProductionSelectorPopup.cpp:92-96` sets `m_bShouldClose` when the click is outside `m_layout`, but `IGameView::HandleMouse` only calls `HandleMouseClick` after `Contains` on that same layout (`include/ui/IGameView.h:55-61`). The branch never runs under normal routing, so maintainers inherit a false contract (“click outside closes”) while clicks outside the popup hit underlying BaseView panels and can open a second selector. Remove the dead branch until modal routing exists, or give the popup a fullscreen hit target with inset chrome.

### [M] Long lists overflow the popup with no clip or scroll
`CacheEntryRects_` walks the full vector with a fixed `lineHeightRatio` and never clamps to `m_layout.height` (`PopTypeSelectorPopup.cpp:22-37`, `ProductionSelectorPopup.cpp:21-36`). With `header_line_offset: 2` and `line_height_ratio: 0.05` that is roughly eighteen visible rows; further entries paint past the chrome and fall outside `Contains`, so they are neither reliably visible nor clickable. Bound the laid-out rows to the content area and add scroll (or reject overflow loudly) before buildings/projects/units push past that limit.

### [M] Click on a row can no-op when callback is empty or pointer is null
Both handlers only close after a successful callback invoke; Production also requires a non-null item (`PopTypeSelectorPopup.cpp:105-110`, `ProductionSelectorPopup.cpp:102-106`). An empty `std::function` or a null `IConstructable*` leaves the popup open with no feedback, against the project rules to throw on unexpected null and to construct only valid objects. Validate non-empty callback (and non-null entries) in the constructor or on click; always dismiss after a hit row is resolved.

### [L] Convention and hygiene items
- `include/ui/base/ProductionSelectorPopup.h:15-16` — comment says “lists buildings” but the type is `IConstructable` (any constructable).
- `include/ui/base/ProductionSelectorPopup.h:7` — unused `#include <string>`.
- `include/ui/base/PopTypeSelectorPopup.h:15` / `ProductionSelectorPopup.h:17` — architecture doc still calls these `UIPopup`; both inherit `UIElement` (no `UIPopup` type exists).
- No tests under `tests/` exercise either popup’s hit-testing, Escape, or empty-list rendering.

**Observed outside slice:**
- `include/ui/IGameView.h:48-62` — mouse routing is hit-test-only; open selectors do not capture outside clicks, so BaseView panels keep receiving input and can stack another popup.
- `docs/architecture/ui-system.md:40-95` — diagram lists only `PopTypeSelectorPopup` (as `UIPopup`) and omits `ProductionSelectorPopup`.
