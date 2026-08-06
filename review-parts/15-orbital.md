## Orbital systems

**Files:** `src/game/orbital/OrbitalAttack.cpp`, `include/game/orbital/OrbitalAttack.h`, `src/game/orbital/OrbitalCensus.cpp`, `include/game/orbital/OrbitalCensus.h`

**Assessment:** A small, readable rules surface: free functions, documented result DTOs, shared
`ReadyYearAfterDeploy`, and tests that pin hit/miss/deploy/census behavior. Error handling is
mostly consistent with project norms (throw on self-ASAT and unknown faction; `bAttempted == false`
for stale UI selections is intentional and documented). The dominant weakness is the destroy
path — a local helper that mirrors intercept destruction, omits secret-project tombstones, and
inherits faction deploy-bookkeeping ambiguities when multiple copies exist.

### [M] Tombstone secret projects destroyed by ASAT
`src/game/orbital/OrbitalAttack.cpp:31-42` — `DestroyOneBuilding_` calls `DestroyBuilding` and
`NotifyBuildingDestroyed` but never `GameState::MarkSecretProjectDestroyed`. Stock orbitals are
not secret projects, but a modded `orbital` + `secret_project` building destroyed on hit (or as
the attacker on fail) becomes buildable again, unlike base-raze tombstoning in
`BaseConquestEffects.cpp:106-111`. `TryAttackSatellite` already has `GameState&`; pass it into
the helper and mark when `pConfig->bIsSecretProject` (same gap exists in intercept — see outside).

### [M] Census count has two algorithms that can drift
`src/game/orbital/OrbitalCensus.cpp:18-32` vs `:49-66` — `BuildOrbitalCensus` tallies by walking
each base and counting `orbital == true` instances, while `CountFactionOrbitalBuildings` checks
one owned config’s `orbital` flag then returns `Faction::CountBuildings` (id match only). Today
every copy of an id shares one registry config, so they agree; a future per-instance flag or a
registry/ownership mismatch would desync the summary grid from `CountOrbitalBuildings`. Have the
single-id count reuse the same per-instance orbital check as `TallyOrbitalBuildings_` (or derive
census entries from repeated calls to one function).

### [L] Convention and hygiene items
- `src/game/orbital/OrbitalAttack.cpp:31-42` — `DestroyOneBuilding_` is a near-copy of
  `InterceptRules.cpp:178-188`; one `Faction` (or shared) helper would keep tombstone/deploy
  policy in a single place.
- `include/game/orbital/OrbitalAttack.h:3` and `include/game/orbital/OrbitalCensus.h:3` — both
  pull `BuildingConfigParser.h` (and thus `nlohmann/json`) for `BuildingConfig_t` /
  `BuildingId_t`; a leaner buildings types header would shrink UI/game include cost.
- `src/game/orbital/OrbitalAttack.cpp:93-95` vs `OrbitalCensus.cpp:56-57` — self-ASAT throws
  `logic_error`, unknown faction throws `runtime_error`; pick one for programmer/precondition
  errors.
- `src/game/orbital/OrbitalCensus.cpp:18-30` — `unordered_map` iteration makes
  `BuildOrbitalCensus` order nondeterministic; harmless for current UI (re-keys by faction+id)
  but brittle for golden tests that compare vectors.
- Test gap: no case for self-ASAT `logic_error` or `CountFactionOrbitalBuildings` on an unknown
  faction id (`tests/game/OrbitalCombatTests.cpp` covers the happy census/ASAT paths only).

**Observed outside slice:**
- `src/game/units/InterceptRules.cpp:178-188` — same missing secret-project tombstone on
  intercept fail-destroy.
- `src/game/Faction.cpp:197-208` — `NotifyBuildingDestroyed` comment claims it prefers a still-
  cooling deploy, but `find_if` takes the first matching id; expired deploys are never purged
  (`DeployBuilding` only appends), so a stale record can be erased instead of an active one when
  a copy is destroyed.
- `docs/architecture/high-level.md` — no orbital/census/ASAT subsystem; satellite UI and these
  free functions are invisible in the architecture diagrams.
