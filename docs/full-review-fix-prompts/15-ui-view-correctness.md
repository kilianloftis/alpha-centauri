# Package 15 — UI: view correctness, null-safety, and per-frame cost

Findings re-verified against the tree at `8d167fe`.

## Scope and split

The package lists four `[H]` and about thirty `[M]`, and its own risk note says to split if the
diff would otherwise be unreviewable. It would be. This lands as **two commits**:

- **A — correctness and null-safety.** Wrong or missing information on screen, dependencies that
  can be null, errors swallowed. Everything a player could observe as a bug.
- **B — per-frame cost.** Snapshots and revision caches for panels that re-query live state per
  element per frame.

Two `[H]` in the review's UI sections are already closed and are not re-done here: *Retire dead
`ComponentSlotDisplay`* (package 14) and *Do not run ProcessTurn from inside Render*
(`WorldView::ProcessPendingAutoEndTurn`, wired from `UIManager::Update`).

## Verified diagnoses — commit A

### [H] Null views and unchecked pushes

`ViewFactory.cpp:61-66, 75-80, 88-93, 106-111` — four `Create*View` methods `return nullptr`
when `GetPlayerFaction()` is missing. `UIManager::PushView` calls `pView->OnPushed()` with no
null check; only the *shortcut* path null-checks before pushing, so `onOpenBase` and friends can
hand it a null `unique_ptr`.

**Chosen:** the factory throws. A missing player faction is not a UI state, it is a broken
session, and every one of these four is reached from a player action that has no meaningful
"nothing happened" outcome. `PushView` throws on a null view as a programmer-error backstop.

### [H] Clicking a garrison steals focus into BaseView

`WorldView.cpp:543-552` — the click handler calls `SelectUnitAtTile_` and then *unconditionally*
opens the base if `FindBaseAt` hits. Verified: every click on a defended base jumps to BaseView
instead of selecting the garrison, and `FindBaseAt` is not owner-filtered, so it also fires on a
rival's base.

**Chosen:** open the base only when the click selected no unit — which is what
`docs/architecture/ui-system.md` already specifies. Selecting a garrison keeps map focus.

### [H] `FormatFactionBonuses` computes and discards

`SocialEngineeringDisplay.cpp:105-126` — the loop resolves each non-zero rating, finds its level
effects, and then does nothing with `pLevelEffects`; `first` is never cleared, so the function
always returns `"None"`. Verified: a wired, player-visible line that silently lies whenever the
faction has any social rating at all.

**Chosen:** format `StatModifier` and `RuleFlag` payloads into the stream. A rating with a
non-zero total whose axis is missing from the registry now throws rather than `continue`s —
consistent with package 11, which made exactly that a load-time error.

### [H] The unit designer ignores `requiredTech`

`UnitDesignerView.cpp:137-144` — `ShowComponentSelector_` offers every registry entry whose
`type` matches, consulting neither the component's `requiredTech` nor the slot's. Verified
against `config/unit_components/`, which gates components on `orbital_spaceflight`,
`planetary_networks` and others. Players can build and save designs that should be locked.

**Chosen:** thread the player's `ResearchManager` into the view and filter the offered list.

## Test coverage — commit A

The UI had no test seam at all, so commit A first landed uncovered. It is covered now; building
the seam was the larger part of this commit.

**Two obstacles, both removed.** `UIManager` named `WorldView` concretely, so linking it into the
test target pulled in the whole view tree, the map renderer and `GameState`. And every view lived
in the executable target next to the SFML backend, so tests could not link one at all.

- `IWorldView` (`include/ui/IWorldView.h`) declares the two things `UIManager` needs beyond
  `IGameView` — `UpdateCameraInput` and `ProcessPendingAutoEndTurn`. `UIManager` holds an
  `IWorldView`, so a fake world view is nine lines.
- `ac-ui` is a new static library holding all 56 UI sources. It depends on the abstract
  `Graphics`/`Input` interfaces and never on a backend, which is what makes it linkable from
  tests; the executable is now `main.cpp` plus `Engine.cpp`.
- `actest::RecordingGraphics` (`tests/RecordingGraphics.h`) records draw *positions*, closing
  package 14's gap where the stub discarded coordinates and an overlapping popup indicator passed
  every test.
- `actest::ViewFixture` (`tests/ViewFixture.h`) supplies a live `GameState`, a player faction, the
  registries `ViewFactory` reads off `GameDataContext`, and the shipped `config/ui/style.json`.

**What it caught immediately:** pruning closed views between the key drain and the mouse drain was
not enough — a keystroke that closed an overlay left it active for the entire following mouse
drain. Pruning is now per event.

Fourteen new tests: eight in `tests/ui/UIManagerTests.cpp`, six in
`tests/ui/ViewFactoryTests.cpp`. `ListSelectorPopupTests` dropped its local stub for the shared
recording one. Every new test was revert-verified against the pre-fix source — the factory test
segfaults on the old null return, which is precisely the reported failure mode.

The `IWorldView` split has a second payoff beyond tests: `ac-ui` no longer compiles against a
rendering backend, so the layering the architecture docs claimed is now enforced by the build.

## Verified diagnoses — commit B

### [M] Base panels force full live yield/production work every frame

`BaseWorkableAreaDisplay.cpp:71-72, 87-89`, `GrowthDisplay.cpp:46-51`,
`ProductionDisplay.cpp:56-66`. Verified: `BuildBaseEffects_` **is** memoized on the pool version
(prior review §1.1), but the getters above it are not — `GetNutrientProduction` and
`GetMineralProduction` each run `ResourceManager::ComputeWorked_` in full (every worked tile,
every supply crawler, the base tile), and the workable-area panel resolves a yield per tile. Two
full passes plus twenty resolutions, sixty times a second, for numbers that move only on a click.

**Chosen:** `BaseDisplaySnapshot_t`, owned by `BaseView`, rebuilt when `ReadBaseDisplayKey`
moves — effects version, `WorkedTileIndex` revision, population revision, home-unit revision,
current production.

**Rejected — a plain per-frame snapshot** (which the review's fix text also offers). It cannot go
stale, but it saves nothing: each panel already calls each getter once per paint, so computing
them together once per frame is the same work. The cost being complained about is that it happens
*at all*, every frame.

**Why tile state is not in the key:** terraforming resolves on turn advance, and `UIManager`
refuses to advance the turn while an overlay covers the map — pinned by the turn-gate test added
in commit A. That invariant is what makes the short key sufficient, and it is written down next
to the struct so a future change to overlay/turn behaviour lands on it.

### [M] Council vote weights recomputed per frame with no revision cache

`CouncilFactionVotesPanel.cpp:119`, `CouncilProposalInfoPanel.cpp:71,121`. `ComputeVoteWeight`
copies the faction's local pool, appends council and world effects, and resolves stat modifiers.
Called once per member in the votes panel and again per member in the info panel's tally: a
five-member council resolved the stat fifteen times per paint.

**Chosen:** `CouncilVoteWeightCache`, one entry per faction, keyed on council revision + local
effects version + population + weighting mode. It carries a `GetComputeCount()` because the
saved work is the entire point and is not observable from the returned weights — the first
version of the test passed against a single-slot cache that recomputed on every read.

**Honest limitation:** the population field is defensive. Every population change I could
construct also moves the faction's local effects version (pops contribute effects), so no test
isolates it. It costs one int compare and removes the dependency on that coupling.

### [M] Satellite summary reallocates census data every frame; view rebuilds on every selection

`SatelliteSummaryPanel.cpp:34-64` rebuilt the orbital-type vector, the faction pointer list and a
string-keyed census map per paint, and `BuildOrbitalCensus` walks every faction's bases.
`SatelliteView.cpp:120-138` called `Rebuild_()` from `SelectFaction_` / `SelectTarget_`,
destroying and recreating the tabs, the Attack button and both list panels — including,
mid-callback, the button that was clicked. `SatelliteButtonListPanel` claimed mutually-exclusive
selection but never updated `m_selectedId` or any button; it only looked right *because* the
parent tore it down.

**Chosen:** the panel fills its grid in `Refresh()`, called from its constructor. Review caught
that the post-attack `Refresh()` I first wired could never fire — the summary panel exists only
in Summary mode, the Attack button only in OrbitalAttack mode — so the call and the pointer it
needed are gone rather than left as documented-but-dead code. The list panel gained
`SetSelected` / `SetItems` and now owns its selection; `Rebuild_()` survives only for a mode
change, which genuinely replaces the layout.

**Also landed here**, being the same files and the same class of defect:
- `OpenAttackerPopup_` threw away a null player faction silently; it now throws, matching the
  factory's policy from commit A.
- `OnAttackClicked_` returned with no feedback when no target was picked, while the Attack
  control stayed enabled — a dead click. It now names what is missing.
- Base panels take `const BaseManager&` / `PopulationManager&` instead of nullable pointers with
  a throw buried in `Render`.
- `BaseView` no longer takes a separate `Faction&`. It was only read by `HandlePopClick_`, which
  is unreachable unless the view is editable, which `ViewFactory` grants only for the player's
  own base — so it could only ever equal `m_rBase.GetFaction()`.

## Test coverage — commit B

Eleven tests, each revert-verified:

- `tests/ui/BaseViewTests.cpp` (4) — the key is stable across paints and moves on an assignment
  change; the snapshot covers exactly the tiles the panel draws; snapshot values equal the live
  getters; and `BaseView` repaints without rebuilding yet still refreshes after a click. Verified
  against both a never-refreshing snapshot and a key missing the worked-tile revision.
- `tests/ui/CouncilVoteWeightCacheTests.cpp` (4) — repeated reads compute once; alternating
  members compute twice, not ten times; a population change is picked up; modes do not share an
  entry. Verified against a single-slot cache and against no caching at all.
- `tests/ui/SatelliteViewTests.cpp` (3) — painting does not re-census but `Refresh()` does;
  selection applies to the panel's own buttons and is exclusive; `SetItems` keeps the requested
  selection.

`RecordingGraphics` now records draw **colours** as well as positions. Selection state in this
UI is often nothing but a fill colour, so without it the list-panel tests could not have seen
the defect at all — the same gap, one layer down, that package 14 hit with positions.

## Review follow-ups applied

Both commits were reviewed together. Nine findings; the two that mattered were the same failure
mode this project keeps hitting.

**The unit-designer fix was half-applied — the worst finding in the package.** Commit A filtered
the *columns* by `requiredTech` but left the save gate (`HasAllMandatory`), the Save button's
enabled state (`DesignStatsDisplay`) and the constructed `UnitDesign` reading the **whole** slot
registry. A slot that is both `required: true` and tech-gated is therefore invisible, unfillable,
and permanently missing: Save is dead for the rest of the game with no explanation, and
`UnitDesign`'s constructor throws on a required slot with no component. Shipped config has no such
slot today, but config is the supported extension point, and my own test fixture had quietly made
the gated slot `required: false` — so the test I wrote could not have caught it. Fixed by building
`m_availableSlots` once in the constructor and routing every consumer through it; the fixture slot
is now `required: true`, and a new end-to-end test fills every visible slot through the real click
path and saves. Revert-verified: pre-fix, the "Fill all required slots" hint never clears and no
design is saved.

**`FormatFactionBonuses` printed enumerator names at the player.** The line no longer lied, but it
read `+20% GrowthRate`. Now split on capitals (`Growth Rate`) rather than adding a second
hand-written table that would drift from `ParseStatId`. The test had hard-coded the defect.

**The base-caching test did not test caching.** It asserted only that two paints draw the same
number of strings — equally true of a snapshot rebuilt every frame, which is the thing being
fixed. `BaseView` now exposes `GetSnapshotBuildCount()` for the same reason the council cache
exposes `GetComputeCount()`. The tile assertions were also self-fulfilling: they picked the live
getter *using* the snapshot's own work state, so a wrong work state could not fail. Work state is
now asserted independently.

**A dead branch I documented as live.** The post-attack `SatelliteSummaryPanel::Refresh()` can
never fire — the summary panel exists only in Summary mode, the Attack button only in
OrbitalAttack mode. Deleted, along with the pointer it needed, and the architecture doc corrected.

**An invariant this package leans on was broken.** `UIManager::Update` called
`ProcessPendingAutoEndTurn` unconditionally, with a comment (mine) asserting `Engine::ProcessTurn_`
would gate it. It does — but only *after* `WorldView` has already cleared the pending flag, so a
queued auto end-turn under an open overlay was silently dropped and never re-armed. Now gated on
`CanAdvanceTurn()`. Pre-existing, but it sits on exactly the turn-gate invariant the base snapshot
key depends on.

**Also:** `HandleGlobalShortcut_` still swallowed a null view, contradicting commit A's policy;
`~BaseView` now releases `m_elements` before `m_snapshot` dies (derived members are destroyed
before the base subobject, so panels briefly held a dangling reference); doc test counts were
wrong; `CreateBaseView` — the view the garrison-click change feeds — had no throw test; and
several comments narrated the change rather than the code.

**Confirmed sound by the review, not changed:** the snapshot key is complete for every mutation
reachable with a base view open; the turn-gate argument holds (`Engine::ProcessTurn_` really does
return early, and `HasOverlayView()` is true for a base view opened from the world screen);
`m_snapshot`'s address is stable across refreshes; both new throws are unreachable on legitimate
paths; and all three `ComputeVoteWeight` call sites were converted.
