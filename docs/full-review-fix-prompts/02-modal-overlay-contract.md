# Package 2 — Modal / overlay contract and turn gating

**Date:** 2026-08-06  
**Source:** [`docs/full-review-fix-packages.md`](../full-review-fix-packages.md) Package 2; findings in [`docs/full-code-review.md`](../full-code-review.md) (Game core yield/UI lifetime; UI — manager; base; selectors; commlinks; council vote; satellite; unit designer; world)  
**Verdict:** **Confirm** the package scope and every listed finding (none fail to reproduce). **Amend** the Yield/UI reconciliation to **consume** Package 1’s landed `Advance` / `StageResult_t::Yield` contract (do not redefine it): keep “no destructive advance under a modal,” but replace the overlay-only `logic_error` with a single `CanAdvanceTurn()` query that also sees **in-view** modals, and rewrite stale `ui-system.md` lifetime text. Outside-click dismiss is fixed by **modal input routing** here; the shared list-selector **widget** extraction stays Package 14. Do **not** split the package.

**Findings that no longer reproduce:** none. Package 1 shifted `TurnProcessor` / `PlayerActions` line numbers and replaced id-ordered resume with a completed-id set, but Yield still exists, `Engine::ProcessTurn_` still asserts `HasOverlayView()`, and every UI modal/stacking/outside-click claim still holds at the loci below.

---

## Verified diagnosis

### 1. Turn `Yield` vs overlay assert — **confirmed [H]** (amend framing)

Package 1 landed: `TurnProcessor::Advance` / `Reset`, completed-faction resume, and `PlayerActions` interaction then order-pass Yield (`include/game/TurnProcessor.h:22-30,45-51`; `src/game/stages/PlayerActions.cpp:51-54,101`; `docs/architecture/turn-system.md:126-131,184-185`).

```98:114:src/game/Engine.cpp
void Engine::ProcessTurn_()
{
    // ...
    if (m_uiManager->HasOverlayView())
    {
        throw std::logic_error("Engine::ProcessTurn_ called while an overlay view is active");
    }

    // Runs until a stage yields for interaction; turn boundaries are handled inside stages.
    m_turnProcessor->Advance(*m_pGameState);
}
```

`HasOverlayView()` is overlay-stack only (`UIManager.cpp:130-133`). Combat already pushes an overlay and skips auto-advance while open (`WorldView.cpp:273-278`). That stack check is a real safety for BaseView popups holding live `Pop&` refs — but:

- `docs/architecture/ui-system.md:217-225` still documents `TurnProcessor::ProcessTurn` and treats the assert as proving turns cannot pause mid-turn (contradicts Package 1).
- In-view modals (`SupplyCrawlPopup` / `ProbeActionPopup` on `WorldView::m_elements`) are invisible to `HasOverlayView`, so `Advance` can still run under them (Enter, End Turn, auto-advance).
- Throwing on a UI-initiated End Turn while a modal is open is the wrong shape; the engine should **ask** whether advance is allowed.

**Requirement (amended):** Consume Package 1 Yield — `ProcessTurn_` remains “call `Advance` when the player resolves the yield reason.” Between Advances, overlays and player UI may exist. **While any modal/overlay that owns the input contract is open, `Advance` must not run.** One query covers overlay stack **and** in-view modals.

### 2. Global shortcuts stack overlays — **confirmed [H]**

```42:72:src/ui/UIManager.cpp
void UIManager::ProcessKeys_()
{
    m_rInput.CaptureKeyAsync([this](KeyEvent_t event)
    {
        IGameView* pActive = GetActiveView_();
        if (pActive && pActive->HandleKey(event))
        {
            return;
        }
        HandleGlobalShortcut_(event.key);
    });
}
// ...
    if (auto pView = it->second())
    {
        PushView(std::move(pView));
    }
```

Overlay views typically consume only Escape; unhandled `F2`/`E`/`U`/… fall through and push another overlay. `CombatView::HandleKey` swallows all keys as a local workaround (`CombatView.cpp:77-81`) — the rule belongs in `UIManager`.

### 3. Council proposal popups stack — **confirmed [H]**

`CommlinksView::OpenCouncilProposals_` always `push_back`s (`CommlinksView.cpp:69-89`) with no already-open guard. Proposals use `top_panel` (ends at view Y ratio `0.65`); Council button under `popup_small` + `council_button_layout` starts at ~`0.652` (`config/ui/style.json:9-10,33-34,338-343`). Default `IGameView::HandleMouse` hit-tests topmost-first by `Contains` (`IGameView.h:55-61`), so the button remains clickable and stacks another popup.

### 4. Escape closes vote view with pending proposal — **confirmed [H]**

```37:40:src/ui/council/CouncilVoteView.cpp
    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
        return true;
    }
```

AI stubs cast on `OnProposalOpened` (`Engine.cpp:327-331`); player Escape leaves `GetPending()` set. `CommlinksView::OnProposalSelected_` refuses Propose while pending (`CommlinksView.cpp:96-98`). Package 5 owns council absentee/resolve rules; Package 2 owns **not dismissing the vote UI while a ballot is open**.

### 5. Orbital popups do not capture input — **confirmed [H]**

`SatelliteView` pushes `OrbitalAttackerPopup` / outcome popup with `popupSmall` (`SatelliteView.cpp:152-178`) and does **not** override `HandleMouse`. Clicks on tabs/Attack/lists still run `SetMode_` / `Rebuild_` / `OpenAttackerPopup_`, erasing or stacking dialogs. `OrbitalAttackerPopup.cpp:185-188` outside-click `Cancel_()` is unreachable under Contains-gated routing.

### 6. WorldView in-view modals do not gate turn or map — **confirmed [H]**

`SupplyCrawlPopup` / `ProbeActionPopup` are pushed onto `m_elements` with raw `Unit*` captures (`WorldView.cpp:320-335,541-571`). `HandleMouse` only forwards to elements that `Contains` the cursor, then falls through to map selection (`WorldView.cpp:404-418`). Enter still calls `m_onProcessTurn()` if the popup does not consume the key (`WorldView.cpp:387-390`; probe/supply `HandleKey` only consume Escape). Auto-advance in `Update_` (from `Render`) likewise ignores open popups (`WorldView.cpp:125-132,273-278`).

### 7. Base / unit-designer / council ballot selectors non-modal — **confirmed [M]**

- `BaseView` pushes selectors (`BaseView.cpp:206-212,225-238`) but does not override `HandleMouse` — underlying panels keep receiving clicks and can stack a second selector.
- `CouncilVoteView::OpenBallotSelector_` always `push_back`s (`CouncilVoteView.cpp:26-28,71,86`); Vote button remains hittable under `popupSmall`.
- `UnitDesignerView::ShowComponentSelector_` always `push_back`s (`UnitDesignerView.cpp:146-150`) with the same Contains-only routing.

### 8. `ProcessTurn` from inside `Render` — **confirmed [M]**

```125:132:src/ui/world/WorldView.cpp
void WorldView::Render(Graphics& rGraphics)
{
    Update_();
    // ...
}
```

`Update_` invokes `m_onProcessTurn()` for auto end-turn (`WorldView.cpp:273-278`), mutating the full turn pipeline mid-paint after the map has drawn and before overlays render.

### 9. Outside-click dismiss dead in three sections — **confirmed [M]**

Same root cause: `IGameView::HandleMouse` / `WorldView::HandleMouse` only call `HandleMouseClick` when `Contains` is true (`IGameView.h:55-61`). Dead branches:

- `ProductionSelectorPopup.cpp:92-96`
- `CouncilProposalsPopup.cpp:105-108`
- `ProbeActionPopup.cpp:84-88` / `SupplyCrawlPopup.cpp:82-86`

(Plus orbital attacker Cancel, same pattern.)

---

## Chosen design

**One modal/overlay contract**, owned by `UIManager` + `IGameView` / `UIElement`, applied by every view that opens a popup. Package 1 Yield is an input: `Advance` runs when the UI says the interaction gate is clear.

### Contract (normative)

1. **Overlay** = entry on `UIManager`’s overlay stack (fullscreen/views: Base, Research, Council vote, Combat, …).
2. **In-view modal** = a `UIElement` marked modal (or recognized as the view’s modal child) while open — selectors, proposals, probe/supply, orbital dialogs, ballots.
3. **Input capture:** While the active view has a top modal element, **all** mouse presses (including outside the chrome rect) and all keys go to that element first; underlying chrome/map must not receive them. Escape dismisses where the popup already supports it (except council vote — see below).
4. **Shortcut suppression:** While `HasOverlayView()`, global view shortcuts must not push. (Optional hardening: replace/dedupe by kind — not required if shortcuts are suppressed.)
5. **Single modal of a kind:** Opening a selector/proposals/ballot/component popup replaces or refuses a second open instance (guard before `push_back`, or dismiss-then-push). Modal capture alone is insufficient where the opener control sits outside the popup rect but is still “under” in z-order for hit-testing — prefer **replace/guard + capture**.
6. **Turn gate:** `UIManager::CanAdvanceTurn()` is false when any overlay is open **or** the world/active view reports an open in-view modal. `Engine::ProcessTurn_` **queries** this and no-ops (or returns without `Advance`) when false — do not throw for a blocked UI End Turn. Keep a loud failure only for true programmer misuse if you retain a debug assert behind the query; the player-facing path must be a soft gate.
7. **Council vote Escape:** While `GetPending()` is set, Escape must **not** set `ShouldClose` (refuse dismiss). Package 5 may later add abstain-on-dismiss / absentee resolve; until then, the vote overlay stays until resolve closes it.
8. **Frame phase:** Auto end-turn / `Advance` must not run from `Render`. Queue a flag in `Update_`/`Render` and consume it from `ProcessInput` (or an `Engine` update step between input and render).

### Modal mark (implementation sketch)

Prefer a small explicit API over per-view snowflakes:

- `UIElement::IsModal() const` (default `false`; popups return `true`), **or** a protected `m_bModal` set in popup ctors.
- `IGameView::HasModalElement() const` / `BlocksTurnAdvance() const` — true if any element reports modal and is not closing.
- Default `IGameView::HandleMouse` / `HandleKey`: if a topmost modal exists, route exclusively to it (mouse always, even when `!Contains`, so outside-click branches become live).
- `WorldView` must use the same rule (override or share helper) so map/End Turn/Enter cannot bypass it; `BlocksTurnAdvance` true while probe/supply modal open.
- `UIManager::CanAdvanceTurn() const` ≈ `!HasOverlayView() && !(worldView && worldView->BlocksTurnAdvance())` (overlays already imply the active view is not WorldView for input, but WorldView still renders underneath — gate both).

### Package 14 coordination

Do **not** extract the shared list-selector widget in this package. Implement the modal flag + default routing so when Package 14 collapses Production/Probe/Supply/Component/Proposals onto one helper, it inherits `IsModal()` and outside-click once. If a temporary helper on `IGameView` (“push unique modal”) reduces copy-paste guards, that is fine; full widget merge is out of scope.

### Docs

Rewrite the Object Lifetime section of `docs/architecture/ui-system.md` for: `Advance` / Yield, `CanAdvanceTurn`, overlay vs in-view modal, and “no destructive mutation while a modal holding live refs is open.” Remove `ProcessTurn` as the live API name.

### Rejected alternatives

| Alternative | Why not |
|-------------|---------|
| Fix each view with a private `HandleMouse` override only | Current state; nine copies will drift (package brief forbids). |
| Push every popup as a full `UIManager` overlay | Heavy; breaks BaseView element ownership and Combat/Base lifetime assumptions; overkill for selectors. |
| Fullscreen invisible hit layer per popup without a shared rule | Works locally but reintroduces divergent dismiss policies; prefer one `IsModal` + default routing. |
| Redefine Yield so overlays are “part of” a stage | Package 1 owns Yield; UI only gates when `Advance` may be called. |
| Keep `logic_error` on `HasOverlayView` as the only gate | Misses in-view modals; throws on ordinary blocked End Turn; docs already point at a query. |
| Wait for Package 14 before fixing outside-click | Player-facing [H]/s ship now; Package 14 should consume the contract, not invent it. |
| Split “turn gate” vs “popup stacking” into two packages | Same missing rule; splitting re-negotiates the contract twice with Package 1/14/5. |
| Abstain-and-resolve on Escape in this package | Touches council lifecycle (Package 5); refuse dismiss is the minimal UI half. |

---

## Interactions with other packages

| Package | Interaction |
|---------|-------------|
| **1** Turn pipeline | **Consumes** `Advance` / Yield / `PlayerActions` two-phase resume. Do not change `TurnProcessor` or stage Yield semantics. `ProcessTurn_` calls `Advance` only when `CanAdvanceTurn()`. |
| **5** Council lifecycle | Package 2 blocks Escape-with-pending on the vote view. Package 5 owns absentee AI, tally exits, and any future “Escape = abstain.” Do not change `PlanetaryCouncil::Resolve` preconditions here beyond what UI gating needs. |
| **14** Shared UI components | Package 2 defines modal + outside-click routing. Package 14 extracts the list-selector and must keep `IsModal` / dismiss behaviour. Avoid merging widgets here. |
| **15** UI view correctness | Null factory/`PushView`, prune-closed-views-out-of-`Render`, per-frame cost, BaseView faction dual-ref — out of scope unless required to implement the modal API. |
| **3** Lifetime | Modal turn-gate protects live `Pop&` / `Unit*` during Advance; destruction protocol remains Package 3. |
| **16** Docs hygiene | Package 2 updates `ui-system.md` modal/lifetime section only; broader doc sweep stays 16. |

---

## Implementation plan

1. **Modal API**
   - Add `UIElement::IsModal()` (or equivalent) and mark all in-view popups: production/pop-type selectors, council proposals/ballot/cooldown as needed, component selector, probe/supply, orbital attacker/outcome.
   - `IGameView`: `HasModalElement` / `BlocksTurnAdvance`; default `HandleMouse`/`HandleKey` exclusive routing to top modal (mouse without Contains gate).
2. **`UIManager`**
   - Suppress `HandleGlobalShortcut_` while `HasOverlayView()`.
   - Add `CanAdvanceTurn()` (overlay stack empty **and** world view not blocking).
3. **`Engine::ProcessTurn_`**
   - Soft-gate on `CanAdvanceTurn()` then `Advance` (Package 1 API). Remove “atomic turn” commentary; document yield + modal gate.
4. **Views — apply contract (no local reinvention of rules)**
   - Guard/replace before push: Commlinks proposals, Council ballot, Base selectors, Unit designer component selector, Satellite attacker (and WorldView probe/supply if re-openable).
   - `CouncilVoteView`: refuse Escape while pending.
   - `WorldView`: modal routing for probe/supply; block Enter / End Turn / auto-advance via `BlocksTurnAdvance`; move auto-advance off the render path.
5. **Outside-click**
   - With exclusive modal mouse routing, existing outside-click branches become reachable; keep Escape dismiss. Do not delete the branches as “dead” once routing lands.
6. **Docs**
   - Update `docs/architecture/ui-system.md` lifetime / ProcessTurn / overlay text to `Advance`, Yield, `CanAdvanceTurn`, overlay vs in-view modal.
7. **Tests**
   - Add requirement-based coverage (see below). Prefer focused unit tests on `IGameView` routing / `UIManager` gates with stub elements/views if full SFML harness is heavy.

---

## Test plan

Requirement-based (assert intended rules; there are **no** existing `tests/ui` suites pinning the buggy behaviour):

1. **Shortcuts suppressed under overlay**  
   With an overlay pushed, a registered shortcut key must not increase overlay depth (or must not call the factory). With no overlay, shortcut still pushes.

2. **`CanAdvanceTurn` false under overlay**  
   After `PushView`, `CanAdvanceTurn()` is false; after pop, true (world present, no in-view modal).

3. **`CanAdvanceTurn` false under in-view modal**  
   World (or test view) with a modal element open → false; after modal closes → true.

4. **`ProcessTurn_` / Advance soft-gate**  
   When `CanAdvanceTurn()` is false, calling the engine turn callback must not advance the turn processor (stage index / mission year unchanged). When true, `Advance` runs (Package 1 contract).

5. **Modal mouse capture**  
   View with a small-rect modal + underlying button: click outside modal chrome hits the modal (outside-dismiss or no-op per popup), not the underlying control. Click cannot open a second stacked modal via the underlying opener.

6. **Modal key capture**  
   With probe/supply-style modal open, Enter must not end the turn; Escape dismisses the modal (where designed).

7. **Council vote Escape**  
   With a pending proposal, Escape does not close `CouncilVoteView` (`ShouldClose` remains false).

8. **Single proposals / ballot / component / base selector**  
   Invoking the open path twice leaves exactly one popup instance (replace or ignore).

9. **Auto end-turn not from Render**  
   Observability: advancing the turn must not be triggered solely by `Render` (e.g. flag set in update/render, consumed in input/update phase). If hard to test without harness, document the phase split and cover via a test double `onProcessTurn` that asserts call timing relative to a fake `ProcessInput`/`Render` sequence on `WorldView` if feasible.

10. **Outside-click dismiss live**  
    ProductionSelector / proposals / probe or supply: with modal routing, a click outside chrome sets should-close (requirement: dismiss works, not that the dead branch text remains forever).

**Existing tests that pinned bugs / outdated contracts and must change:**

| Test / doc assertion | Why it must change |
|----------------------|--------------------|
| None under `tests/` assert shortcut stacking, overlay End Turn throw, or in-view modal gating | N/A — add new tests; do not weaken Package 1 `TurnProcessor` Yield tests. |
| `docs/architecture/ui-system.md` Object Lifetime (`ProcessTurn`, overlay-only assert) | Rewrite for `Advance` / Yield / `CanAdvanceTurn` / in-view modals — requirement change vs doc, not a test. |
| If any future test expects `ProcessTurn_` to `logic_error` on overlay | Replace with soft-gate / `CanAdvanceTurn` requirement. |

Do **not** weaken Package 1 yield/resume tests.

---

## AI implementation prompt

```markdown
# Implement Package 2 — Modal / overlay contract and turn gating

You are working in the Alpha Centauri C++ rebuild at `/home/martok/alpha-centauri`.

## Goals

1. **One modal/overlay contract** on `UIManager` + `IGameView` / `UIElement`:
   - Overlay = `UIManager` overlay stack entry.
   - In-view modal = `UIElement` that reports modal while open.
   - While a modal is topmost on the active view: exclusive mouse (including outside chrome) and key routing to that element; underlying controls/map must not run.
   - Global view shortcuts must not push while an overlay is active.
   - Opening a second proposals/ballot/base-selector/component/orbital/probe/supply popup must replace or refuse — no stacks.

2. **Turn gate consumes Package 1 Yield.** `Engine::ProcessTurn_` calls `TurnProcessor::Advance` only when `UIManager::CanAdvanceTurn()` is true (no overlay **and** no blocking in-view modal on the world view). Soft-gate blocked advances (do not throw for ordinary UI End Turn while modal). Do **not** redefine `StageResult_t::Yield` or change `PlayerActions` / `TurnProcessor` resume semantics.

3. **Council vote:** Refuse Escape dismiss while a proposal is still pending (`GetPending()` set). Do not close the vote overlay on an open ballot. (Package 5 owns absentee/AI resolve rules.)

4. **WorldView:** Gate Enter, End Turn, and auto-advance while probe/supply (and any other) in-view modals are open. Move auto end-turn off the `Render` path into input/update phase.

5. **Outside-click:** Make existing outside-dismiss branches reachable via modal mouse routing (do not “fix” by deleting the branches without routing). Coordinate with Package 14 by putting the rule in shared view/element infrastructure, not by extracting the list-selector widget here.

6. **Docs:** Update `docs/architecture/ui-system.md` lifetime section for `Advance` / Yield / `CanAdvanceTurn` / overlay vs in-view modal. Remove live references to `TurnProcessor::ProcessTurn` as the API.

## Constraints

- Follow `.cursor/rules/coding-guidelines.md`: SOLID, references over pointers, throw over silent defaults for unexpected nulls, no legacy shims.
- Build and test **only** via `./bd` (never raw cmake/make/ctest).
- Read and follow: `docs/full-review-fix-prompts/02-modal-overlay-contract.md` (verified diagnosis + design). Findings origin: `docs/full-code-review.md`. Package brief: `docs/full-review-fix-packages.md` Package 2.
- Package 1 has landed — yield contract is in `docs/architecture/turn-system.md` and `01-turn-pipeline-integrity.md`. **Consume it.**
- Prefer one shared rule applied everywhere over per-view special cases (CombatView’s “swallow all keys” may remain as belt-and-suspenders but must not be the only fix).

## Analysis reference

`docs/full-review-fix-prompts/02-modal-overlay-contract.md`

## Primary files

- `include/ui/UIManager.h`, `src/ui/UIManager.cpp`
- `include/ui/IGameView.h`, `include/ui/UIElement.h`
- `src/game/Engine.cpp` (`ProcessTurn_`)
- `src/ui/world/WorldView.cpp`, `include/ui/world/WorldView.h`
- `src/ui/world/ProbeActionPopup.cpp`, `src/ui/world/SupplyCrawlPopup.cpp`
- `src/ui/base/BaseView.cpp`, selector popups under `src/ui/base/`
- `src/ui/commlinks/CommlinksView.cpp`, `CouncilProposalsPopup.*`
- `src/ui/council/CouncilVoteView.cpp`, `CouncilBallotPopup.*`
- `src/ui/satellite/SatelliteView.cpp`, orbital popups
- `src/ui/unit-designer/UnitDesignerView.cpp`, `ComponentSelectorPopup.*`
- `docs/architecture/ui-system.md`
- Tests: add under `tests/` (new UI/manager/view routing tests as needed); do not weaken `tests/game/TurnProcessorTests.cpp` Yield coverage

## Acceptance criteria

- [ ] `UIElement`/`IGameView` expose a clear modal contract; default routing captures input for top modal including outside clicks.
- [ ] Global shortcuts do not stack overlays while an overlay is already active.
- [ ] `CanAdvanceTurn()` is false for overlay stack **or** world in-view modal; `ProcessTurn_` soft-gates then calls `Advance`.
- [ ] Probe/supply open: Enter / End Turn / auto-advance do not call `Advance`; map chrome under the popup does not mutate selection/orders.
- [ ] Base / unit-designer / council proposals / ballot / orbital: cannot stack a second popup via underlying controls; guard or replace.
- [ ] Council vote Escape refused while pending.
- [ ] Outside-click dismiss works for ProductionSelector, council proposals, and probe/supply (and orbital cancel) under the new routing.
- [ ] Auto end-turn is not invoked from `Render` paint path.
- [ ] `ui-system.md` documents Advance/Yield + modal gate (no atomic-`ProcessTurn` story).
- [ ] Requirement-based tests for the above; `./bd test` passes for affected suites.
- [ ] Package 1 Yield / `PlayerActions` behaviour unchanged.

## Out of scope

- Extracting the shared list-selector widget / scrollable list merge (Package 14).
- Council absentee AI, tally, `Resolve` precondition redesign (Package 5) beyond UI Escape gating.
- ViewFactory null returns, prune-closed-views out of `Render`, TileHitTester, Forest hardcoding (Packages 14/15).
- Changing `TurnProcessor`, stage order, or Yield semantics (Package 1 — already done).
- Base destruction / live `BaseManager&` invalidation (Package 3).
- Election candidate list / governor rules UI (Package 5).

## What NOT to do

- Do not redefine or “simplify away” `StageResult_t::Yield` / `PlayerActions` phases.
- Do not fix modal behaviour only inside individual views without a shared `UIManager`/`IGameView`/`UIElement` rule.
- Do not extract ProductionSelector clones into a shared widget in this package (Package 14).
- Do not leave `Engine::ProcessTurn_` throwing `logic_error` as the only turn gate, and do not keep gating solely on `HasOverlayView()` while in-view modals exist.
- Do not dismiss `CouncilVoteView` on Escape while a proposal is pending.
- Do not call `Advance` / `ProcessTurn_` from `WorldView::Render`.
- Do not delete outside-click dismiss code as “unreachable” without first making modal routing deliver those clicks.
- Do not weaken Package 1 turn-processor yield tests.
- Do not expand into Package 15 null-safety / per-frame polish unless required for the modal API.
```
