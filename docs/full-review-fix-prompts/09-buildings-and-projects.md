# Package 9 — Buildings, secret projects, orbital census

**Source package:** [`docs/full-review-fix-packages.md`](../full-review-fix-packages.md), Package 9
**Verified against:** working tree at commit `473b062` (after full-review Packages 1–8)

**Status: complete.** The `[H]` and all nine `[M]`s are fixed; the `[L]` hygiene block is batched
into package 16.

---

## Done

### [H] Secret-project uniqueness is only checked when the build menu is generated — FIXED

`IsCompleted` had exactly one production caller: `GetBuildingsAvailableForConstruction`, which
only filters the list the UI shows. The path that actually grants a building —
`ProductionManager::CompleteProduction` → `BaseManager`'s `OnProductionCompleted` handler →
`BuildingManager::AddBuilding` — checked nothing. Two bases that both had a project listed when
they queued it (same faction, or two factions in one `BaseProduction` pass) would both complete
it, silently breaking "only one faction in the world may own this building".

`AddBuilding` is the single mutation point, so the invariants live there now: `allowMultiple`
and secret-project uniqueness (against the availability calculator, which covers both "someone
owns it" and "it was destroyed"). A secret project in a base with no availability calculator
throws rather than guessing.

### [M] `IsCompleted` also answers true for projects that no longer exist — FIXED

Split into the two questions it was conflating:
- `IsUnavailable` — can no longer be built (owned *or* tombstoned). What the build menu and
  `AddBuilding` need.
- `IsOwnedByAnyFaction` — a base holds it right now. What a UI "owned by ⟨faction⟩" label, a
  score, or a victory check needs. False for a destroyed project.

### [M] Tombstone secret projects destroyed by ASAT — FIXED, and unified

`OrbitalAttack::DestroyOneBuilding_` destroyed and notified but never marked the tombstone, so a
modded `orbital` + `secret_project` building became buildable again — unlike the same building
razed with its base. The same gap existed in intercept fail-destruction and in probe sabotage
(the latter is the half package 8 explicitly left here). All three now mark the tombstone, so
every destruction path shares one rule.

### [M] Census count has two algorithms that can drift — FIXED

`BuildOrbitalCensus` tallied by walking each base counting `orbital` instances;
`CountFactionOrbitalBuildings` checked the config then called `CountBuildings`. Two definitions
of one number. The second now answers from the same tally as the first.

### [M] `BuildingConfig_t` has no default member initialisers — FIXED

`category`, `mineralCost`, `allowMultiple` and `bIsSecretProject` were uninitialised, so a
parser that forgot a field — or a struct built by hand in a test — started from indeterminate
values. All members now have defaults.

### [M] The config struct lives in the parser header, so `nlohmann/json.hpp` leaks everywhere — FIXED

`BuildingConfig_t` moved to a new `game/buildings/BuildingConfig.h`. `BaseManager`,
`BuildingManager`, the orbital census, probe effects and the UI all reach this type and none of
them parse JSON.

### [M] Typo'd or wrong-shaped keys are silently defaulted; parse failures name neither the building nor the file — FIXED

`json::value()` substitutes the default for a key of the wrong shape, so `"allow_multiple":
"yes"` parsed as `false` and the modder was told nothing; a typo'd key was accepted and ignored,
so the setting silently never applied. The parser now type-checks each key when present and
rejects unknown keys, naming the building and the key. (File naming comes from
`JsonConfigLoader`, which already wraps parse failures with the path.)

### [M] `category` is mandatory in the parser, undocumented, and never read — FIXED

Confirmed unread: the only reference outside the parser is the assignment itself. Made optional
rather than deleted — `config/buildings.json` ships it on every entry and the build menu is the
obvious future consumer — so a modded building is no longer forced to supply a value that does
nothing. A comment records that it should be tightened when something reads it.

### [M] `BuildingRegistry` skips the validation extension point it inherits — FIXED

`Validate_` now runs the whole-set checks the base class exists to host: a `secret_project` that
is also `allowMultiple` is a contradiction the availability rules cannot express, and a negative
`mineral_cost` is rejected. Both used to surface at runtime, or not at all.

---

## Note on the fixture change

`world_beacon` and `test_facility_a` gained `allow_multiple: true`. Two existing tests add a
second copy of each on purpose (effect-pool memo invalidation, and the deploy-record
cooling-preference case), which the new `AddBuilding` rule correctly rejects. Marking them
stackable makes the tests' premise legal rather than weakening the rule — which is separately
pinned by `BuildingInvariantTests`.
