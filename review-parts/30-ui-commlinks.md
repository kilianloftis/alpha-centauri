## UI — commlinks

**Files:** `src/ui/commlinks/CommlinksPanel.cpp`, `include/ui/commlinks/CommlinksPanel.h`, `src/ui/commlinks/CommlinksView.cpp`, `include/ui/commlinks/CommlinksView.h`, `src/ui/commlinks/CouncilButton.cpp`, `include/ui/commlinks/CouncilButton.h`, `src/ui/commlinks/CouncilCooldownPopup.cpp`, `include/ui/commlinks/CouncilCooldownPopup.h`, `src/ui/commlinks/CouncilProposalsPopup.cpp`, `include/ui/commlinks/CouncilProposalsPopup.h`

**Assessment:** Thin, readable overlay: panel lists known factions, view owns council open/propose flow, popups mirror other selector UIs. The dominant weakness is fragile modal layering and silent failure paths in `CommlinksView` — proposal UI can stack, dismiss-outside never fires under current hit-testing, and missing-commlinks / pending-vote rejects close the list with no player feedback (unlike the cooldown path).

### [H] Prevent stacking multiple council proposal popups
`src/ui/commlinks/CommlinksView.cpp:69-89` — `OpenCouncilProposals_` always `push_back`s a new `CouncilProposalsPopup` with no “already open” guard. With `config/ui/style.json` layouts, `top_panel` ends at view Y ratio `0.65` while the Council button (under `popup_small` + `council_button_layout`) starts at `0.652`, so `IGameView::HandleMouse` still hits `CouncilButton` while a proposals popup is up and opens another copy. Guard before push, or make the proposals UI a full-window modal hit target that covers the button.

### [M] Do not silently drop proposal selection on gated failures
`src/ui/commlinks/CommlinksView.cpp:92-109` — After the list entry is chosen, `CouncilProposalsPopup` closes itself (`CouncilProposalsPopup.cpp:115-118`). `OnProposalSelected_` then returns with no UI when `GetPending()` is set, or when `HasCommlinksToAllMembers` fails. Cooldown correctly opens `CouncilCooldownPopup_`; the other propose-time gates leave the player with a vanished list and no explanation. Surface a notice (or keep the list open) for pending and missing-commlinks the same way cooldown is handled.

### [M] Outside-click dismiss on proposals popup is unreachable
`src/ui/commlinks/CouncilProposalsPopup.cpp:105-108` — Click-outside sets `m_bShouldClose`, but `IGameView::HandleMouse` only calls `HandleMouseClick` when the cursor is inside the element’s layout (`include/ui/IGameView.h:55-62`), so that branch never runs. Escape works; the written dismiss path is dead. Use a fullscreen modal layout so outside chrome is still this element, or delete the dead branch and treat Escape as the only dismiss.

### [M] Wire or drop unused `playerCooldownYears`
`include/ui/commlinks/CouncilCooldownPopup.h:19` / `src/ui/commlinks/CouncilCooldownPopup.cpp:23` — `ProposeCooldownYears` is passed in (`CommlinksView.cpp:63`) and stored as `m_playerCooldownYears`, but `Render` never draws it; the body shows member/governor intervals and years remaining only. Callers look like they supply player-facing cooldown data that is discarded — render “your cooldown” or remove the parameter.

### [L] Convention and hygiene items
- `include/ui/commlinks/CouncilButton.h:4` — unnecessary `#include "graphics/Graphics.h"` (forward declare / cpp-only).
- `src/ui/commlinks/CouncilButton.cpp:27-32` — `HandleMouseClick` does not require `MouseButton_t::Left` (unlike the two council popups).
- `src/ui/commlinks/CouncilCooldownPopup.cpp:33` / `CouncilProposalsPopup.cpp:24` — both restyle from `Style().productionSelectorPopup`, so production UI tweaks silently restyle council chrome; prefer a dedicated style block.
- `src/ui/commlinks/CouncilCooldownPopup.cpp:36-37` — OK button size uses hardcoded `0.35f` / `1.4f` instead of style/config.
- `src/ui/commlinks/CommlinksPanel.cpp:33-37` / `CommlinksView.cpp:52-55` — null `GetPlayerFaction()` / council returns quietly; guidelines prefer throw when a player-facing view requires them.
- `include/ui/commlinks/CouncilCooldownPopup.h:11` — comment says popup shows when opening council on cooldown; it actually opens after selecting a proposal while on cooldown (`CommlinksView.cpp:100-103`).

**Observed outside slice:**
- `include/ui/IGameView.h:55-62` — hit-test-before-click makes every popup’s “click outside layout to close” pattern dead unless the element is fullscreen.
- `docs/architecture/ui-system.md` — omits `CommlinksView` / council propose UI entirely (architecting rule expects diagram updates for new components).
