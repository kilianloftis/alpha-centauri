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

Fourteen new tests: nine in `tests/ui/UIManagerTests.cpp`, five in
`tests/ui/ViewFactoryTests.cpp`. `ListSelectorPopupTests` dropped its local stub for the shared
recording one. Every new test was revert-verified against the pre-fix source — the factory test
segfaults on the old null return, which is precisely the reported failure mode.

The `IWorldView` split has a second payoff beyond tests: `ac-ui` no longer compiles against a
rendering backend, so the layering the architecture docs claimed is now enforced by the build.

## Deferred to commit B

Per-frame recomputation: base panels calling live yield/production getters per element, council
vote weights recomputed per frame with no revision key, the satellite summary reallocating census
data per frame, and the satellite view rebuilding wholesale on each selection change.
