# Current game state: read through the thread's game state, write through `GameState&`

Based on `origin/main` at `96119e2` (Moisture generation update). Lands after phase 0, which fixes
the four tests that fail at `96119e2` and adds the readable-code rules to
`.devin/rules/coding-guidelines.mdc`. Lands before `docs/plans/code-organisation.md`.

## Context

### What we have

- Code that is not handed a `GameState` reaches it through a faction. `GameState::AttachToSession_` binds every faction to the session (`BindGameState`) and to its world effects (`BindWorldEffects`).
- Seven production call sites read the session back-pointer (`Faction::GetGameState()`):
  - `DiplomaticPermissionRules::SessionOf_`
  - `BaseEcology::RequireGameState_`
  - `FactionEffectsPool` (twice)
  - `CommerceManager` (twice)
  - `TerraformRules`' `RunCompleteEffects_`
- Six of the seven only read. The terraform on-complete effects write, through a `TriggeredEffectContext_t` built on the session.
- Three of the readers throw when the faction is unbound. The two caches treat an unbound faction as having no session.
- `Faction::EnsureComposedEffects_` reads world extras through the bound `IWorldEffectsSource`.
- `AtrocityEffects` reads settings through the perpetrator faction, although it already takes `GameState&`.
- Some public functions take `const GameState&` only to read it. Many take `GameState&` because they write.
- The game is single-threaded today. AI players are planned on worker threads, so the current game state is per thread, not per process.

### Target rules

1. **Each thread has a current game state.**
   - `CurrentGameState()` returns it as `const GameState&`, and throws when the thread has none.
   - `FindCurrentGameState()` returns it, or null.
2. **`GameStateScope` installs a game state on the current thread for the scope's lifetime.** When it is destroyed it restores whatever was current before. Scopes are stack objects and nest.
3. **Constructors never install a scope.**
   - `Engine` installs one for the game it runs.
   - A test installs one for the game state it exercises.
   - Work on another thread installs one for the game state it reads.
4. **Reads.**
   - Code that is not handed a `GameState` reads it through `CurrentGameState()`.
   - Public functions and methods that only read the game do not take a `GameState` parameter.
   - A free function that needs ruleset data it is not handed reads `CurrentGameState().GetGameData()`. Classes take the ruleset pieces they use in their constructors.
   - Code that runs outside a game (loading, validators, world generation) takes `GameDataContext` explicitly.
   - Exception: `ProbeActionExecutor`'s queries keep `GameState&`, because they build `ProbeTarget_t`, which holds mutable references for `TryProbeAction`.
5. **Writes.** Code that changes state outside its own object takes `GameState&`. A `GameState&` parameter means the function may write.
6. **Game objects never hold a `GameState`.** That covers factions, bases, units and their parts.
   - Services that `GameState` owns keep the reference `GameState` gives them: `TileEffectsContext`, `Pathfinder`, `StepEvaluator`, the executors and `SecretProjectAvailabilityCalculator`.
   - One exception stays for now: the unit-id allocator callback that `GameState` binds into each faction (`BindUnitSpawnServices`). It carries a TODO to re-examine.
7. **Objects never cross game states by pointer.**
   - Anything handed between game states or threads names factions, bases and units by id.
   - Only the thread that installed a game state writes to it.
8. **Nothing reads `CurrentGameState()` during `GameState`'s own constructor.**

Game behaviour does not change.

## Changes

Commit straight to `main` after phase 0. Each numbered step may land as several smaller commits. Every commit builds, passes `./bd test` and starts the game (see Verification).

### Code style

Every new or changed line follows the six hard rules at the top of `.devin/rules/coding-guidelines.mdc`:
1. One call deep: name intermediate values.
2. Comments say why, in two lines or fewer, with no history and no references to plans or reviews.
3. No guards for cases that cannot happen.
4. No pass-through functions: when logic moves, its callers call the new home.
5. No flag parameters.
6. Nothing speculative.

Moved code keeps its body unless the move already touches a line. Each commit's diff is checked against these rules before it lands.


### 1. The current game state

- New `include/game/CurrentGameState.h` and `src/game/CurrentGameState.cpp`:
  - `const GameState& CurrentGameState()` throws `std::logic_error` when the thread has no current game state.
  - `const GameState* FindCurrentGameState()`.
  - `class GameStateScope`:
    - `explicit GameStateScope(const GameState&)`
    - not copyable or movable
    - the destructor restores the game state that was current when the scope was built
  - The current pointer is a `thread_local` in the `.cpp`.
- New `tests/game/CurrentGameStateTests.cpp`:
  - with no scope, `CurrentGameState()` throws and `FindCurrentGameState()` is null
  - a scope makes its game state current
  - a nested scope restores the outer game state when it is destroyed
  - a scope installed on one thread is not visible on another
- `docs/architecture/high-level.md`: new section "Current game state" with Target rules 1–8.
- New `.devin/rules/game-state-access.mdc` in Cursor's rule format:
  - frontmatter: `description`, and `alwaysApply: true`
  - body: rules 4–7 in short form
  - `CLAUDE.md` imports it

### 2. `Engine` and tests install scopes

Nothing reads the current game state yet, so this commit cannot change behaviour.

- **`Engine`**:
  - New member `std::unique_ptr<GameStateScope> m_pGameStateScope`, declared after `m_pGameState` so it is destroyed first.
  - `StartNewGame_` creates it right after `m_pGameState`, before any faction is created.
- **`FactionFixture`**: a `GameStateScope` member for `pBindState`, declared after it and created right after it in the constructor.
- **`MakeLandSession`** is unchanged. Its four callers, `DiplomacyFixture` among them, install a scope beside the state they keep, as every other test does.
- **`ViewFixture`**: a scope member for `pState`, declared after it.
- **Every other test that constructs a `GameState`** installs a `GameStateScope` on the next line.
  - At `96119e2`, tests construct a `GameState` 53 times in 34 files. Three of those are the helpers above.

### 3. Factions stop holding the game state

- `Faction` loses `m_pGameState`, `BindGameState`, `GetGameState`, `m_pWorldEffects` and `BindWorldEffects`.
  - `EnsureComposedEffects_` takes the world composition stamp and world extras from `FindCurrentGameState()`, as an `IWorldEffectsSource`.
  - With no current game state there are no world extras, as with an unbound faction today.
- `GameState::AttachToSession_` no longer binds the faction to itself or to world effects. `FactionFixture` drops the same two calls.
- `Faction::BindUnitSpawnServices` stays, with a TODO comment above its declaration:
  ```cpp
  // TODO: Re-examine; this callback is the faction's last link to its GameState.
  // Suggestion: move the unit-id counter onto WorldMap and allocate through the map.
  ```
- **Readers of the old back-pointer:**
  - `DiplomaticPermissionRules` uses `CurrentGameState()`. `SessionOf_` is deleted.
  - `BaseEcology` uses `CurrentGameState()`. `RequireGameState_` is deleted, and `CollectInputs_` / `CollectRevisions_` read the current game state.
  - `FactionEffectsPool` and `CommerceManager` use `FindCurrentGameState()`. With no current game state there are no pair effects and no commerce, as today when the faction is unbound.
  - `AtrocityEffects` reads settings from `rGameState.GetSettings()`.
- **Terraform on-complete effects go through the executor's session surface.**
  - New `IUnitOrderWorld::ApplyTerraformCompleteEffects(Tile&, const TerraformProject_t&, Unit& rFormer, std::mt19937&)`. `GameState` implements it with today's `RunCompleteEffects_` body.
  - `ApplyTerraformResult` takes `IUnitOrderWorld*`. As today, it throws when the project declares `on_complete_effects` and none is given.
  - `UnitOrderExecutor` passes `m_pWorld`. `IUnitOrderWorld.h` forward-declares `TerraformProject_t`.
- **`BaseIntegrationTests`:**
  - "Production completion writes on_complete SetInfiltration into the DiplomacyLedger" drops `REQUIRE(beneficiary.GetGameState() == &state)` and installs a scope for `state`. Its header comment names the scope where it named `BindGameState`. This is a requirement change, because the binding no longer exists.
  - "Building production without spawn services completes without on_complete effects" replaces `REQUIRE(fixture.pOwnerFaction->GetGameState() == nullptr)` with `REQUIRE(FindCurrentGameState() == nullptr)`.
- **Which game state fixture factions now read:**
  - Inside a `FactionFixture`, `BaseFixture`'s owner faction reads `pBindState`, like the fixture's other factions.
  - In a test that constructs its own `GameState`, fixture factions read that game state while its scope is installed.
  - Each test whose outcome changes is reported. It is changed to keep its factions in one game state.
- Update `docs/architecture/faction-system.md` and `docs/architecture/effects-system.md`.

### 4. Reads stop taking `GameState`

These drop their `const GameState&` parameter and read `CurrentGameState()`. Every caller updates.

- `InfiltrationRules`: both `FactionFilterCoversTarget` overloads, and `HasInfiltration`
- `TriggeredEffectDispatch`: `UnitHasHoldLink`, `HoldLinkHostName`
- `FoundBaseRules`: `CanFoundBaseAt(const Tile&, FactionId_t)`, the overload that took the game state
- `OrbitalCensus`: `BuildOrbitalCensus`, `CountFactionOrbitalBuildings`
- `DiplomaticProposalRules`: `IsValidProposal`
- `DiplomaticPermissionRules`: `StatusRulesFor`, `IsObligedToDefend`
- `CommerceCalculator`: `ComputeAllLines`, `ComputeForFaction`, `ComputeForBase`
- `TerraformRules`: `CanStartTerraform`, `TerraformEnergyCost`
- `UnitOrderExecutor::CanStartTerraformProject`
- `PlanetaryCouncil`: `HasCommlinksToAllMembers`, `YearsUntilCanPropose`

## Tests

- Commit 1 adds the `CurrentGameState` tests above.
- Every other test follows the signature changes and keeps what it asserts. The two exceptions are the `BaseIntegrationTests` changes listed in commit 3.
- Any test that fails is reported by name, with whether it was a requirement change or a bug.

## Verification

- After each commit:
  - `./bd all` (with `--no-sfml` when SFML is unavailable), then `./bd test`.
  - Start the game from the repo root with `timeout 15 stdbuf -oL ./alpha-centauri`. It must print `Starting game loop...` and still be running when `timeout` stops it (exit status 124). Startup runs `LoadGameData` on `config/`, the only check of the shipped config.
- Stop and report any failure.
