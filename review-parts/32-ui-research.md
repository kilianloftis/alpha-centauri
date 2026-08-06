## UI — research

**Files:** `src/ui/research/CurrentResearchPanel.cpp`, `include/ui/research/CurrentResearchPanel.h`,
`src/ui/research/ResearchView.cpp`, `include/ui/research/ResearchView.h`

**Assessment:** This is a thin, readable overlay: `ResearchView` owns one panel and closes on
Escape; `CurrentResearchPanel` draws label/target/progress from style config with no
business logic of its own. The dominant weakness is presentation correctness and null
handling — the panel shows config tech ids to the player and treats a missing
`ResearchManager` the same as “no research target,” which sibling panels already reject
loudly.

### [M] Do not render a null `ResearchManager` as “None”
`src/ui/research/CurrentResearchPanel.cpp:26` — `if (m_pResearch && m_pResearch->HasResearchTarget())`
collapses a null manager and a valid manager with no target into the same “None” branch, so a
wiring bug is invisible in the UI. Project guidelines prefer references and throwing on
unexpected null; `GrowthDisplay` already throws when its manager pointer is null
(`src/ui/base/GrowthDisplay.cpp:22-25`). The 2026-07-09 const-correctness fix left nullable
`const ResearchManager*` parameters; the silent null branch is still wrong. Direction: take
`const ResearchManager&` in both constructors (or throw if a pointer must remain) and leave
“None” only for `!HasResearchTarget()`.

### [M] Show the tech display name, not the config id
`src/ui/research/CurrentResearchPanel.cpp:28` — `DrawText(m_pResearch->GetResearchTarget(), …)`
paints `TechId` (e.g. `ethical_calculus`, `secrets_of_the_human_brain`). Config carries a
separate player-facing `name` (`TechConfig_t::name`, e.g. “Ethical Calculus”). That is wrong
output for a “Current Research Target” label, not a missing feature. Direction: render the
display name once `ResearchManager` exposes the current `TechConfig_t` (or its `name`); see
outside-slice note.

### [L] Convention and hygiene items
- `include/ui/research/ResearchView.h:18` — `m_pResearch` is stored and never read; the
  constructor forwards the parameter to `CurrentResearchPanel` and never uses the member.
- `include/ui/research/CurrentResearchPanel.h:16` — empty `HandleMouseClick` override duplicates
  the base default in `UIElement` and adds nothing.
- `src/ui/research/ResearchView.cpp:16-24` — unlike `SocialEngineeringView`, does not call
  `IGameView::HandleKey` before handling Escape, so future element key handlers on this view
  would never run.
- `include/ui/research/CurrentResearchPanel.h:5` — unused `#include <string>`.
- `src/ui/research/ResearchView.cpp:3` — unused `#include "graphics/Graphics.h"`.
- `include/ui/research/ResearchView.h:14` — `explicit` on a two-parameter constructor is a no-op.

**Observed outside slice:**
- `include/game/faction/ResearchManager.h:26` — `GetResearchTarget()` returns only `TechId`; there
  is no accessor for the current `TechConfig_t` / display `name` the research panel needs.
- `src/ui/ViewFactory.cpp:77-81` — `CreateResearchView` returns `nullptr` when there is no player
  faction; callers must tolerate a null view from the F2 shortcut factory.
