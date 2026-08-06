## Turn stages — late pipeline and custom

**Files:** `src/game/stages/CustomTurnStage.cpp`, `include/game/stages/CustomTurnStage.h`,
`src/game/stages/Save.cpp`, `include/game/stages/Save.h`, `src/game/stages/TurnEnd.cpp`,
`include/game/stages/TurnEnd.h`, `src/game/stages/Upkeep.cpp`, `include/game/stages/Upkeep.h`,
`src/game/stages/VictoryConditionChecks.cpp`, `include/game/stages/VictoryConditionChecks.h`,
`src/game/stages/WorldEvents.cpp`, `include/game/stages/WorldEvents.h`

**Assessment:** `Save`, `TurnEnd`, `Upkeep`, and `VictoryConditionChecks` are honest
placeholder stages (log + `Continue`) and are fine as such. `CustomTurnStage` is a thin,
correctly typed hook-only fallback with a useful construction guard. The only stage in this
slice with real rules is `WorldEvents`, and its RNG and year-index choices are the dominant
weakness: forest/kelp growth is live gameplay that does not participate in the game's RNG
ownership model and hardcodes a year epoch already owned elsewhere.

### [H] WorldEvents forks a private RNG from year and map area alone
`src/game/stages/WorldEvents.cpp:24-30` builds a fresh `std::mt19937` seeded only from
`turnIndex` and `width * height`, then passes that into `SpreadTerraformImprovements`.
`GameState` already owns a shared combat/probe stream (`m_rng` at
`include/game/GameState.h:196`, seeded once in `GameState.cpp:48`), but WorldEvents never
touches it — and there is no accessor today, so the stage invents a second deterministic
universe. Consequence: every game with the same map dimensions gets the identical
forest/kelp sample sequence every year, independent of map contents or the game's random
device seed. Direction: drive spread from the game RNG (expose it on `GameState` if needed)
so world growth shares the same stream policy as combat and probes; do not re-seed from
year × area each turn.

### [M] First-playable-year epoch is hardcoded beside GameState's starting year
`src/game/stages/WorldEvents.cpp:23-24` subtracts literal `2100` to form SMAC's
`CurrentTurn`-style index. `GameState.cpp:30-32` already documents and owns
`k_StartingMissionYear = 2099` so the first `TurnStart` increment lands on 2100. Those two
constants are not linked: changing the starting year (or making it config) silently skews
spread attempt rates via `TerraformSpreadGrowthAttempts` without a compile error. Direction:
share one named epoch (constant or `GameState` helper such as years-since-start) and use it
here.

### [M] Custom stage "at least one hook" guard does not enforce its own invariant
`include/game/stages/CustomTurnStage.h:14` and `CustomTurnStage.cpp:10-15` exist so a
hook-only stage cannot "silently do nothing", but `RequireAtLeastOneHook` only checks that
the pre/post/replace *lists* are non-empty. `HookContext` treats presence as
`!vector.empty()` (`HookContext.cpp:62-74`) and only invokes `callback` when it is set
(`:55-58`). Nothing in the repo ever assigns `Hook_t::callback`, so the shipped
`CustomModStage` entry (`config/turn_stages.json:135-148`) passes construction, skips
`ExecuteImpl` via replace-hook presence, and every turn becomes a cout-only no-op. This is
the Custom*-side incomplete edge of prior 1.10 / the turn-pipeline replace-hook finding:
tighten the guard to require a callable callback (or fail config load when `script_path`
cannot be bound), so an empty mod stage fails at startup instead of sitting in the default
turn order.

### [L] Convention and hygiene items
- `include/game/stages/CustomTurnStage.h:26,39` — `m_name` is stored after construction only
  for the ctor error path; once `RequireAtLeastOneHook` returns it is unread.
- `include/game/stages/CustomTurnStage.h:19,32` — `name` is a `const std::string&` parameter
  without the project `r` reference prefix (`rName`).
- `include/game/stages/CustomTurnStage.h:23,36` — `ExecuteImpl` bodies live inline in the
  header while every sibling stage in this slice keeps `ExecuteImpl` in the `.cpp`.
- `src/game/stages/WorldEvents.cpp:25-30` — binds a `const WorldMap&` solely for the seed,
  then calls non-const `GetWorldMap()` again for the spread; one mutable reference is enough.
- `src/game/stages/WorldEvents.cpp:28` — `GetWidth() * GetHeight()` is signed `int`
  multiplication before the `unsigned` cast; large maps hit UB before seeding.
- `src/game/stages/Save.cpp`, `TurnEnd.cpp`, `Upkeep.cpp`, `VictoryConditionChecks.cpp` —
  four identical stub shapes (ctor + cout + `Continue`); fine while empty, but any shared
  logging/TODO pattern should be one helper before a fifth copy appears.
- No stage-level test exercises `WorldEvents::ExecuteImpl` (only
  `SpreadTerraformImprovements` in `tests/effects/TileEffectsTests.cpp`); the year→index and
  RNG wiring above are therefore unguarded.

**Observed outside slice:**
- `docs/architecture/turn-system.md:205-207` still lists `WorldEvents` among placeholder
  logging stages; it now mutates the map via terraform spread and the diagram/docs should
  say so.
- `docs/architecture/turn-system.md:220` still says `Engine` increments the mission year
  after `ProcessTurn`; `TurnStart` does that now (`TurnStart.cpp:24`).
- `src/game/council/CouncilOutcomeApplier.cpp:39` still expects a WorldEvents trigger API
  for sea-level / world-parameter outcomes; none exists on this stage yet (unimplemented,
  but the coupling is already documented).
- `GameState::m_rng` has no public accessor (`include/game/GameState.h:196`); fixing the
  WorldEvents RNG finding needs a small API addition there or an equivalent injection point.
