# Package 16 — Architecture docs, mod seams, and hygiene sweep

Findings re-verified against the tree at `0d1d947`, after packages 1–15 and 17 landed the
structural changes this package documents.

## Scope and split

Two `[H]`, one `[M]`, two `[L]`. Lands as **three commits**:

- **A — the documented graph matches the live graph.** Pure documentation, no behaviour change.
- **B — mod seams get a real consumer.** Hooks that can see the game, faction events that are
  actually bridged, and one worked example that would break if either regressed.
- **C — hygiene sweep.** The residual naming / dead-code bullets.

The review lists two further `[H]` in this section — the base lifetime protocol and reconciling
turn `Yield` with the UI contract — that the package table does not claim. Both were largely
closed by earlier packages (`Faction::TransferBaseTo` / `ExtractBase` and `BaseView`'s
owner-change check; `UIManager::CanAdvanceTurn` and the turn gate). What remains of them is
documentation, which commit A covers.

## Verified drift — commit A

Every claim below was checked against the tree, not taken from the review.

### `faction-system.md`

- **`FactionManager` does not exist.** It is in the diagram (`:13`), three edges (`:84`, `:86`,
  `:87`), a style rule (`:147`), and a performance claim ("provides O(1) faction lookup by ID",
  `:377`). Factions live in `GameState::m_factions`.
- **`Faction::ProcessTurn()` does not exist** (`:337`). There is no such method; turns are stage
  driven through `TurnProcessor::Advance`.
- **The subsystem call list is fiction** (`:338-341`): `Economy::CalculateIncome()`,
  `Military::UpdateUnits()`, `Research::AdvanceResearch()`, `Diplomacy::UpdateRelations()`. None
  of those four methods exist. `Military` holds designs only; there is no per-faction `Diplomacy`
  at all.
- **`HookSystem` does not exist** (`:357`). The live type is `HookContext`.

### `high-level.md`

- **`SFMLUIManager` / `NullUIManager`** (`:419-420`) — `UIManager` is a single concrete class;
  the SFML dependency is in `Graphics`/`Input` backends, and since package 15 the whole UI is the
  backend-free `ac-ui` library.
- **`HookSystem`** (`:232`, `:431`) — `HookContext`, and it is turn-stage scoped.
- **`FactionFactory`** — already corrected in place (`:318` explicitly says there is no such
  thing), so only the stale mentions elsewhere need removing.
- **`TileBonusRegistry` / `config/tile_bonuses.json`** — the file does not exist; tile bonuses are
  improvement placement through `TileBonusGeneration`.

### `research-system.md`

- **`TechRegistry` labelled "global singleton"** (`:12`). It is a member of `GameDataContext`,
  constructed and loaded by `LoadGameData` and passed by reference. The word invites exactly the
  design this codebase spent packages 2–4 removing.

### `turn-system.md` / `ui-system.md`

- `ProcessTurn` versus `Advance` + `StageResult_t::Yield`. `ui-system.md:304` describes
  `Engine::ProcessTurn_` accurately (it is the engine's own method name); the turn doc's use of
  `ProcessTurn` for the *processor* is the drift.

### Missing entirely

- **No architecture document for diplomacy or trade**, though `.devin/rules/architecting.md`
  calls for a fine-grained diagram per major subsystem. Package 17 changed trade validation
  semantics and had nothing to update. This is the one place where the fix is to write a document
  rather than correct one.

## Verified diagnoses — commit B

### [H] Mod seams have no consumer

Three separate half-built seams, each verified:

1. **Hooks cannot see the game.** `Hook_t::callback` is `std::function<void()>`
   (`HookContext.h:15`) — no `GameState`, no `Faction`, no stage identity. A hook can therefore
   do nothing except side-effect through captured state, which no config-driven hook has.
2. **`scriptPath` cannot be used.** `TurnStageConfigParser` parses it and then *throws* if it is
   non-empty ("script loading is not available; remove the hook or bind a callback in C++"). That
   is better than silently ignoring it — the review's "parsed and never loaded" reading was
   checked and is not what the code does — but it means a hook is reachable only from C++, so a
   config-only mod still cannot exist. The project already embeds a `LuaRuntime` (tech cost, pop
   composition), so the gap is the scripting API, not the runtime.
3. **`EventBridge` bridges bases only, and its TODO is stale.** The constructor comment says
   faction signals wait "once Faction gains a `FactionId_t`" — `Faction::GetFactionId()` has
   existed for many packages. Meanwhile `event-system.md` describes faction events as bridged.

**Chosen:** give `Hook_t::callback` a context struct, wire the faction signals `EventBridge`
already claims, and add one worked sample mod exercising both — a turn-stage hook and a gameplay
event handler — as a test, so a regression in either seam fails the suite rather than being
noticed by the first modder.

**Not done:** loading Lua from `scriptPath`. The runtime exists, but "what a mod script may call"
is an API design question, and inventing a scripting surface here would be making up the very
mechanics the guidelines forbid guessing at. The context struct is the prerequisite either way;
recorded as the next step rather than half-built.

## Verified diagnoses — commit C

### [L] Naming

- **Three enum classes without `_t`**: `DiplomaticProposeResult`, `DiplomaticStatus`,
  `DiplomaticActionKind`. Renamed across 10 files.
- **Seven private parser methods without a trailing `_`**: `ParseStageConfig`, `ParseHooks`,
  `ParseFactionDirectory`, `ParseBuildingConfig`, `ParseRatingConfig`, `ParseProposalConfig`
  (and its three siblings), `ParseTechConfig`, `ParsePolicyConfig`, `ParseCategory`.

**Not done — the `r`-prefix sweep on reference parameters.** About fifty parameters in headers
take a reference without the `r` prefix (`buildingId`, `layout`, `effect`, `ctx`, …). A textual
rename cannot distinguish a reference *parameter* from a same-named local variable in the same
function, and renaming a local to `rFoo` would introduce a fresh violation in the other
direction while the compiler stayed silent. Doing it properly needs the rename driven from the
AST, not from grep. Recorded rather than half-applied — this is exactly the class of sweep where
a mechanical edit produces a large, unreviewable, behaviour-free diff with silent errors in it.

### [L] Dead code

Sixteen public methods had a declaration and a definition and **no caller anywhere**, tests
included. Fourteen were deleted:

`ConsumePsych`, `WorldDisplay::SetCameraOffset` / `GetCameraX` / `GetCameraY` / `GetVisibleCols`
(all superseded by `MapViewport`), `HookContext::HasPreHooks` / `HasPostHooks`,
`UIManager::HasViews`, `UiStyle::IsLoaded`, `Pop::IsPlayerAssignable`,
`StepEvaluator::HasHostileUnit` / `HasVisibleHostileUnit`, `Faction::GetDiscoveredBuildings`,
`MapViewport::IsInView`, `WorkerAssignmentManager::ReleaseAllUserAssignments`.

Two were **kept and given a consumer instead**, because they are seams rather than leftovers:

- `WorkerAssignmentManager::SetTileScorer` — a declared customization point. An uncalled seam is
  how the other three seams in this package came to be half-built, so the sample-mod test now
  replaces the scorer and asserts the assignment follows it.
- `DiplomaticActionExecutor::Reject` — the other half of the one-slot pending-proposal contract
  that package 17 strengthened with `Busy`. Without `Reject` a declined proposal would block
  every later one forever. Now tested.

Two architecture docs named `SetCameraOffset` as a control point and were corrected.

## Review follow-ups applied

Reviewed inline (the multi-agent review is unavailable — the account's monthly spend limit was
reached during package 17).

**One correction to this document's own diagnosis:** I recorded `scriptPath` as "parsed and never
used", following the review. It is actually parsed and *rejected* with an explicit message. The
seam is closed deliberately rather than leaking silently, which is the better of the two
behaviours; the text above now says so.

**Checked and sound:**
- Every `Hook_t::callback` assignment in the tree takes the new `HookArgs_t` parameter; the
  compiler enforced this, since the signature change breaks any missed site.
- The stage id set by the parser reaches the constructed stage: `TurnStageFactory` passes
  `config.hookContext` into every creator, including the two `Custom*TurnStage` paths, so a hook
  on a config-declared custom stage sees its own id.
- `WireFaction` captures the faction **id**, not the object, so a bridged handler cannot dangle
  if the faction moves; `WireBase`'s existing lambdas capture the base by reference, which is
  safe because the wiring is keyed on that same object's lifetime.
- Deleting fourteen methods changed no test outcome, which is the point: they had no callers.
  The build is the check — a missed reference is a link error, not a silent behaviour change.
