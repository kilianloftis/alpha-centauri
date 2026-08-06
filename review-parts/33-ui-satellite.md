## UI — satellite / orbital

**Files:** `src/ui/satellite/OrbitalAttackOutcomePopup.cpp`, `include/ui/satellite/OrbitalAttackOutcomePopup.h`, `src/ui/satellite/OrbitalAttackerPopup.cpp`, `include/ui/satellite/OrbitalAttackerPopup.h`, `src/ui/satellite/SatelliteButtonListPanel.cpp`, `include/ui/satellite/SatelliteButtonListPanel.h`, `src/ui/satellite/SatelliteLabeledButton.cpp`, `include/ui/satellite/SatelliteLabeledButton.h`, `src/ui/satellite/SatelliteSummaryPanel.cpp`, `include/ui/satellite/SatelliteSummaryPanel.h`, `src/ui/satellite/SatelliteView.cpp`, `include/ui/satellite/SatelliteView.h`

**Assessment:** The slice is a clear, small composition — `SatelliteView` owns mode/selection state, list panels and labeled buttons are thin, and the deferred post-attack rebuild (`m_bPendingAttackRefresh`) correctly avoids destroying the attacker popup mid-callback. The dominant weakness is missing modal input ownership: popups sit in the same element list as tabs/lists, so clicks outside the small popup rect still drive underlying controls and can tear down or stack dialogs.

### [H] Capture input while orbital popups are open
`src/ui/satellite/SatelliteView.cpp:157-178` / `src/ui/satellite/SatelliteView.cpp:199-301` — `OrbitalAttackerPopup` / `OrbitalAttackOutcomePopup` use `Style().layouts.popupSmall` (40%×40% rect). `SatelliteView` does not override `HandleMouse`, so `IGameView::HandleMouse` only delivers clicks to elements whose bounds contain the point. Clicks on tabs, Attack, or faction/target lists while a popup is open still run `SetMode_` / `OnAttackClicked_` / `SelectFaction_` / `SelectTarget_`, which call `Rebuild_()` and erase the open popup, or push a second attacker popup on top of the first. `OrbitalAttackerPopup.cpp:185-188` outside-click `Cancel_()` is unreachable for the same reason. Fix: while a popup is topmost, route mouse (and optionally keys) only to it — including outside clicks — and refuse selection/mode rebuilds until it closes.

### [M] Rebuild the entire view on every selection change
`src/ui/satellite/SatelliteView.cpp:120-138` / `src/ui/satellite/SatelliteView.cpp:199-301` — `SelectFaction_` and `SelectTarget_` always `Rebuild_()`, destroying and recreating tabs, Attack, and both list panels. `SatelliteButtonListPanel` claims mutual-exclusive selection but never updates `m_selectedId` or button selected state in place (`SatelliteButtonListPanel.cpp:45-50`); it only works because the parent tears the UI down. Prefer updating list selection locally (or a narrow refresh) so open chrome and any future transient state survive.

### [M] Attack actions fail silently when prerequisites are missing
`src/ui/satellite/SatelliteView.cpp:141-147` — `OnAttackClicked_` returns with no feedback if faction or target is unset, while the Attack control stays enabled. `src/ui/satellite/OrbitalAttackerPopup.cpp:39-47` — `Confirm_()` likewise no-ops when nothing is selected; the Attack button is always created enabled (`OrbitalAttackerPopup.cpp:85-89`). Players get a dead click instead of disabled chrome or a message (the empty-attacker path already uses `ShowOutcome_`).

### [M] Summary panel reallocates census data every frame
`src/ui/satellite/SatelliteSummaryPanel.cpp:34-64` — each `Render` rebuilds the orbital-type vector, faction pointer list, and a string-keyed `unordered_map` of census counts. Same class of per-frame UI rebuild that was previously removed from world/base displays. Cache until census/factions/registry inputs change, or compute in the view when entering Summary mode.

### [M] Null player faction is swallowed
`src/ui/satellite/SatelliteView.cpp:160-164` — `OpenAttackerPopup_` returns quietly when `GetPlayerFaction()` is null. Guidelines require throwing on unexpected null rather than silent no-ops; a missing player during this view is not a normal empty state (contrast the explicit outcome string used when attackers are empty at line 169).

### [L] Convention and hygiene items
- `include/ui/satellite/SatelliteView.h:3` — header pulls `BuildingConfigParser.h` only for `BuildingId_t`; prefer a lighter alias header.
- `src/ui/satellite/SatelliteView.cpp:258` — faction ids round-trip through `std::to_string` / `std::stoi` instead of a typed list API.
- `src/ui/satellite/OrbitalAttackOutcomePopup.cpp:43-48` / `OrbitalAttackerPopup.cpp:105-106` — popup padding/font ratios borrowed from `productionSelectorPopup`, coupling satellite chrome to an unrelated style block.
- `src/ui/satellite/SatelliteLabeledButton.h:19` — `SetSelected` is unused; selection changes rebuild buttons instead (`OrbitalAttackerPopup.cpp:59-63`).
- `src/ui/satellite/OrbitalAttackOutcomePopup.cpp:69-72` — null checks on `m_pOkButton` after the constructor always creates it.
- No UI/unit tests under `tests/` exercise satellite view selection, popup stacking, or attack confirm flow (game-layer ASAT is covered separately).

**Observed outside slice:**
- `include/ui/IGameView.h:55-62` — default `HandleMouse` Contains-gates all elements, so outside-click dismiss in other popups (`ProductionSelectorPopup`, `PopTypeSelectorPopup`, etc.) is similarly unreachable without a per-view override.
- `docs/architecture/ui-system.md` — does not mention `SatelliteView` or the satellite panels despite the architecting rule to keep diagrams current.
