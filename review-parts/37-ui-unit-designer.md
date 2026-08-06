## UI — unit designer

**Files:** `src/ui/unit-designer/ComponentSelectorPopup.cpp`, `include/ui/unit-designer/ComponentSelectorPopup.h`, `src/ui/unit-designer/ComponentSlotDisplay.cpp`, `include/ui/unit-designer/ComponentSlotDisplay.h`, `src/ui/unit-designer/DesignListPanel.cpp`, `include/ui/unit-designer/DesignListPanel.h`, `src/ui/unit-designer/DesignStatsDisplay.cpp`, `include/ui/unit-designer/DesignStatsDisplay.h`, `src/ui/unit-designer/SlotColumnPanel.cpp`, `include/ui/unit-designer/SlotColumnPanel.h`, `src/ui/unit-designer/UnitDesignerView.cpp`, `include/ui/unit-designer/UnitDesignerView.h`, `src/ui/unit-designer/UnitStatusPanel.cpp`, `include/ui/unit-designer/UnitStatusPanel.h`, `include/ui/unit-designer/UnitDesignerState.h`

**Assessment:** The view is well factored into slot columns, stats, design list, and status panels, with layout driven by `UnitSlotRegistry` and UI style config. Dominant weaknesses are a dead parallel slot widget (`ComponentSlotDisplay`), missing tech gating despite populated `required_tech` data, and draft/selection state that diverges after the player edits a loaded design. Popup and list overflow behavior will also trap maintainers as catalogs grow.

### [H] Ignore component `requiredTech` when listing and saving
`src/ui/unit-designer/UnitDesignerView.cpp:137-144` — `ShowComponentSelector_` pushes every registry entry whose `type` matches; nothing consults `UnitComponentConfig_t::requiredTech` (or slot `requiredTech`). Production config already gates many components (e.g. `orbital_spaceflight`, `planetary_networks`), and other systems treat empty/`HasDiscoveredTech` as the availability contract. Players can assemble and `HandleSaveDesign_` designs that should be locked. Filter the available list (and optionally hide locked slots) against faction research; thread a research/availability dependency into the view constructor.

### [H] Retire or reuse dead `ComponentSlotDisplay`
`src/ui/unit-designer/ComponentSlotDisplay.cpp:1-64`, `src/ui/unit-designer/SlotColumnPanel.cpp:116-150` — `ComponentSlotDisplay` is compiled and styled but never constructed; `UnitDesignerView` builds `SlotColumnPanel` instead, which reimplements the same fill/border/label/name paint and click callback. Architecture docs still describe per-slot `ComponentSlotDisplay` children. Two UIs for one job will drift. Either compose `SlotColumnPanel` from `ComponentSlotDisplay` children or delete the unused class and its style block, and update the diagram.

### [M] Component selector is not modal; popups can stack
`src/ui/unit-designer/UnitDesignerView.cpp:146-150`, `include/ui/IGameView.h:55-61` — each slot click `push_back`s a `ComponentSelectorPopup` without dismissing an existing one. `IGameView::HandleMouse` only delivers clicks to an element that `Contains` the cursor, so clicks outside the small popup reach slots/stats underneath and open another popup. Close any open selector before opening a new one, or make the popup consume outside clicks (dismiss or no-op).

### [M] Selected design and draft state desync after edits
`src/ui/unit-designer/UnitDesignerView.cpp:104-116`, `src/ui/unit-designer/UnitStatusPanel.cpp:48-66` — `OnDesignSelected_` sets `m_pSelectedDesign` and copies components into `m_state`. Later slot changes update only `m_state`; selection is left pointing at the saved design. `UnitStatusPanel` then shows that design’s name/active count while slots and `DesignStatsDisplay` show the edited draft, and `DesignListPanel` keeps its highlight. Clear selection (and list highlight) when the draft diverges, or explicitly enter an “editing design X” mode.

### [M] Design list silently truncates overflow designs
`src/ui/unit-designer/DesignListPanel.cpp:61-88` — boxes are laid out horizontally and `Render` `break`s when the next box would exceed panel width; there is no scroll or overflow cue. Extra designs are invisible and unreachable (hit-testing never receives clicks outside the panel). Add horizontal scroll/paging, or wrap, matching `SlotColumnPanel`’s scroll pattern.

### [M] Unknown slot `column` values fall through to left
`src/ui/unit-designer/UnitDesignerView.cpp:65-72` — only `"right"` is recognized; any other string (including typos) is treated as left. Config documents `"left"` or `"right"`. Throw (or validate at slot-registry load) on unexpected `column` instead of silently misplacing slots.

### [L] Convention and hygiene items
- `include/ui/unit-designer/DesignStatsDisplay.h:16-17` / `DesignListPanel.h:16` — prefer references over nullable pointers for always-valid deps (`UnitDesignerState_t`, slots, `Military`); guidelines require throw-on-null, not unchecked deref (`DesignStatsDisplay.cpp:58`).
- `include/ui/unit-designer/DesignListPanel.h:25` — `SetSelectedDesign` is public but never called; selection only updates inside `HandleMouseClick`.
- `include/ui/unit-designer/UnitDesignerView.h:38` — uses `std::function` / `std::string` without `#include <functional>` / `<string>` (transitive today).
- `src/ui/unit-designer/DesignStatsDisplay.cpp:70` — builds a temporary `UnitDesign` every `Render` frame; cache on state change or share with the save path.
- `src/ui/unit-designer/ComponentSelectorPopup.cpp:31-35` — entry rects can extend past popup height with no scroll/clip; same growth hazard as the design list.

**Observed outside slice:**
- `docs/architecture/unit-designer-system.md` — still describes `ComponentSlotDisplay` ownership and a `nullptr` UnitManager TODO; code uses `SlotColumnPanel` and `ViewFactory` already passes `GetUnitManager()` (`src/ui/ViewFactory.cpp:118`).
- Tech-availability filtering will also need a research/faction dependency wired at `ViewFactory::CreateUnitDesignerView` once the view filters `requiredTech`.
