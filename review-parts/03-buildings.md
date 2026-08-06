## Buildings and secret projects

**Files:** `src/game/buildings/BuildingConfigParser.cpp`, `include/game/buildings/BuildingConfigParser.h`, `include/game/buildings/BuildingRegistry.h`, `src/game/buildings/SecretProjectAvailabilityCalculator.cpp`, `include/game/buildings/SecretProjectAvailabilityCalculator.h`

**Assessment:** This is a small, readable slice: the parser is 20 lines of straight-line field
reads on top of the shared `JsonConfigLoader`/`ConfigFields` helpers, and the registry gets
duplicate detection and throwing lookups for free from `Registry<>`. The dominant weakness is
that neither file defends the rules it owns — the parser accepts anything the JSON happens to
contain and defaults the rest, `BuildingRegistry` declines the `Validate_` hook that exists
precisely for entry-level invariants, and the secret-project rule is only ever asked about at
menu-build time, so nothing enforces it at the moment a project is actually completed.

### [H] Secret-project uniqueness is only checked when the build menu is generated
`src/game/buildings/SecretProjectAvailabilityCalculator.cpp:16` — `IsCompleted` has exactly one
production caller: `BuildingManager::GetBuildingsAvailableForConstruction`
(`src/game/faction/base/buildings/BuildingManager.cpp:79-83`), which only filters the list the UI
shows. The path that actually grants a building —
`ProductionManager::CompleteProduction` → `BaseManager`'s `OnProductionCompleted` handler
(`src/game/faction/base/BaseManager.cpp:107-115`) → `BuildingManager::AddBuilding`
(`.../BuildingManager.cpp:24-31`) — performs no check at all. Two bases that both had the
project in their list when they selected it (same faction, or two factions in the same
`BaseProduction` pass) will both complete it, violating the stated rule "only one faction in the
world may own this building" (`config/buildings/README.md:22`). The same hole lets a base finish
a project that was tombstoned by `MarkSecretProjectDestroyed` after it was selected. The fix is
to make this calculator the authority consulted at grant time as well — e.g. give it a throwing
`RequireBuildable(id)` that `AddBuilding` calls — rather than leaving the rule to a UI filter.

### [M] `category` is mandatory in the parser, undocumented, and never read
`src/game/buildings/BuildingConfigParser.cpp:27` — `ParseGameCategoryField` uses `j.at(key)`
(`src/game/GameCategory.cpp:47`), so `category` is a hard requirement, yet the documented schema
does not list it (`config/buildings/README.md:17-24`) and neither documented example includes it.
A modder copying the README example gets a bare
`[json.exception.out_of_range.403] key 'category' not found` at startup. Meanwhile
`BuildingConfig_t::category` is written here and read nowhere in `src/` — only
`TechConfig_t::category` feeds anything (`src/game/faction/ResearchSelector.cpp:121`); the sole
reader of the building field is `tests/game/GameCategoryParserTests.cpp:41`. Either give it a
consumer and document it, or drop the field and stop making every building config carry it.

### [M] Typo'd or wrong-shaped keys are silently defaulted instead of rejected
`src/game/buildings/BuildingConfigParser.cpp:28-31` — every optional field goes through
`json::value(key, default)`, which cannot distinguish "absent" from "misspelled". `"minerals_cost"`
yields `mineralCost == 0` and a building that costs the clamped minimum of 1 mineral
(`ProductionCostCalculator.cpp:18`); `"secretproject"` yields a Secret Project that everyone can
build; `"orbital"` misspelled removes a satellite from the public census. This is the mod-facing
entry point for a whole directory of merged JSON files, and it contradicts the project's
"prefer throwing over returning default values" rule. The parser already demonstrates the right
shape for one key (the `required_techs` rejection at `:32-37`) — generalise it by rejecting any
key not in the known set.

### [M] Parse failures name neither the building nor the file
`src/game/buildings/BuildingConfigParser.cpp:25-39` — `ParseId`, `ParseGameCategoryField`, and the
`value()` type-mismatch path all propagate raw nlohmann exceptions. Since `LoadPath` merges every
`*.json` in `config/buildings/` (`include/lib/config/JsonConfigLoader.h:59-82`), the operator sees
`key 'id' not found` with no file name, no array index, and no id. `config.id` is parsed first, so
wrapping the remainder of `ParseBuildingConfig` in a `catch`/rethrow that prepends
`"building '<id>'"` is cheap and would cover every field at once.

### [M] `IsCompleted` also answers true for projects that no longer exist
`src/game/buildings/SecretProjectAvailabilityCalculator.cpp:18-21` — a razed project is
tombstoned in `GameState` and reported as "completed" forever. That is the right *availability*
answer, but the method name and its header comment ("has been completed in any base of any
faction", `SecretProjectAvailabilityCalculator.h:19`) both promise something else. The first
caller that wants the honest question — a UI "owned by <faction>" label, a score or victory
check, diplomacy — will read this as ownership and be wrong. Rename to something like
`IsUnavailable`, or split the tombstone check from the ownership scan.

### [M] `BuildingConfig_t` has no default member initialisers
`include/game/buildings/BuildingConfigParser.h:20-26` — `category`, `mineralCost`, `allowMultiple`
and `bIsSecretProject` are uninitialised while `orbital` alone gets `= false`. The parser assigns
all of them so it is safe today, but the struct is default-constructed outside the parser:
`tests/faction/BuildingTechGateTests.cpp:28` and `:38` set only `id`/`requiredTech` and leave
`mineralCost` — which `GetBaseCost()` hands to the production system — indeterminate. Give every
member an initialiser; the project rule is that a constructed object is valid.

### [M] `BuildingRegistry` skips the validation extension point it inherits
`include/game/buildings/BuildingRegistry.h:9` — the class is an empty derivation (justified, since
an alias could not be forward-declared), but it never overrides `Validate_`, unlike `TechRegistry`
(`include/game/research/TechRegistry.h:16`), `PopTypeRegistry` and `CouncilProposalRegistry`. That
is the natural home for the whole-set checks buildings currently lack: `secret_project` combined
with `allow_multiple` is self-contradictory (once built, `IsCompleted` blocks it anyway, so the
flag is dead), and nothing rejects a negative `mineral_cost`. Instead, building validation is
spread over two free functions in `src/game/` that the composition root must remember to call.

### [M] The config struct lives in the parser header, so `nlohmann/json.hpp` leaks everywhere
`include/game/buildings/BuildingConfigParser.h:16-43` — `BuildingConfig_t` is defined next to the
parser, and the header includes `<nlohmann/json.hpp>`. Eighteen files include it, most of which
want only the data struct: `include/game/effects/ActiveEffect.h`, `include/game/faction/base/BaseManager.h`,
`include/game/orbital/OrbitalCensus.h`, `include/ui/satellite/SatelliteView.h`. The project already
splits these elsewhere (`UnitComponentConfig.h`, `SocialPolicyConfig.h`); a `BuildingConfig.h`
holding the struct would keep the JSON dependency inside the two parsing translation units.

### [L] Convention and hygiene items
- `include/game/buildings/BuildingConfigParser.h:23,26` — `allowMultiple` and `orbital` lack the mandated `b` prefix while the neighbouring `bIsSecretProject` has it.
- `src/game/buildings/BuildingConfigParser.cpp:11-13` and `include/game/buildings/BuildingConfigParser.h:48-49` — a user-provided empty constructor and a defaulted destructor on a class with no state; `ParseBuildingConfig` touches no member, so the whole class could be free functions in a namespace.
- `include/game/buildings/SecretProjectAvailabilityCalculator.h:19` — takes `const std::string&` where the rest of the building API uses the `BuildingId_t` alias.
- `src/game/buildings/SecretProjectAvailabilityCalculator.cpp:28` — dereferences `pBuilding` unchecked while every other loop over the same container null-checks it (`BuildingManager.cpp:59`, `OrbitalCensus.cpp:25`, `BaseConquestEffects.cpp:61`). The invariant does hold (entries come from `Registry::Get`); pick one policy — ideally state the invariant here and drop the defensive checks elsewhere.
- Test gap: the only coverage of this calculator is the tombstone case (`tests/game/BaseConquestTests.cpp:471`). Nothing asserts the primary rule — that a project completed in another faction's base disappears from `GetBuildingsAvailableForConstruction`.
- Prior review item 4.3 records `BuildingConfig_t`'s `IConstructable` inheritance and its embedded `IsAvailable` rule logic as deliberately deferred; not re-reported.

**Observed outside slice:**
- `src/game/units/ProbeActionEffects.cpp:114-128` — probe sabotage picks a random building excluding only the hardcoded `"Headquarters"`, so it can destroy a Secret Project without calling `MarkSecretProjectDestroyed`; the project then becomes buildable again, contradicting the raze tombstone at `src/game/units/BaseConquestEffects.cpp:106-111`.
- `src/game/faction/base/buildings/BuildingManager.cpp:24-31` — `AddBuilding` enforces nothing: not the tech gate, not `allowMultiple`, not secret-project uniqueness. It is the single point where all three could be enforced.
- `src/game/orbital/OrbitalAttack.cpp:40` and `src/game/units/InterceptRules.cpp:187` — same missing tombstone if an `orbital` building is ever also a Secret Project.
- `docs/architecture/high-level.md:270` — still says definitions load from `config/buildings.json`; it has been the directory `config/buildings/` since the multi-file loader landed.
- `config/buildings/README.md:17-24` — the field table omits the required `category`, and both worked examples (`:112`, `:129`) would fail to load as written.
