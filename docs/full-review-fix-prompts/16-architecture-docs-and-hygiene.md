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
2. **`scriptPath` is parsed and never used.** `TurnStageConfigParser` fills it; nothing loads it.
   The project has a `LuaRuntime` already (used for tech cost and pop composition), so the gap is
   wiring, not capability.
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
