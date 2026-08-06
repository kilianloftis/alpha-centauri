# Package 5 — Parser strictness & honored shapes

**Source package:** [`docs/effects-fix-packages.md`](../effects-fix-packages.md) § Package 5  
**Review:** [`docs/effects-model-review.md`](../effects-model-review.md)  
**Date:** 2026-08-04  
**Verdict:** Confirm Pass 1 (strictness) then Pass 2 (dispatch refactor) as separate changes. Amend proposal allowlist to include deferred `WorldParameter` (see Design decision).

---

## Verified diagnosis

Two silence layers: the shared effect parser invents values or aborts instead of throwing; council/governor parsers accept shapes the runtime drops.

### BonusEffectParser — required keys via `operator[]`

`ParseEffectConfig` reads required keys with nlohmann const `operator[]`:

```305:307:src/game/effects/BonusEffectParser.cpp
    const std::string typeStr = effectJson["type"];
    const std::string scopeStr = effectJson["scope"];
    const std::string persistenceStr = effectJson.value("persistence", "Continuous");
```

Missing `"type"` / `"scope"` → debug assert abort / release `end()` dereference, not `std::runtime_error`. Rest of the file throws with messages. `ParseEffects` does not pre-check keys (`683:693:src/game/effects/BonusEffectParser.cpp`). `tests/effects/ParserTests.cpp` has no missing-`type` case.

### `ParseEffectConfig` monolith

`301:666:src/game/effects/BonusEffectParser.cpp` — one ~365-line if/else over 18 types. Repeated tile-resource-stat predicates (`435`, `445`, `463`); scope legality only for Infiltration / TileResourceCap / OrbitalAttack / TransportParams. `radius` accepted on any scope (`312`) though only `TileEffectReaches` consumes it and requires `TileLocal` (`86:90:src/game/effects/ActiveEffect.cpp`) — e.g. `radius` on `ThisBase` parses and is inert forever.

### Silent balance defaults (parser + structs)

| Key | Parser default | Struct default |
|-----|----------------|----------------|
| `TileResourceCap` `max` | `ParseNumber(..., 2.0)` `:470` | `max = 2` `BonusEffect.h:188` |
| `OrbitalAttack` `chance` | `50.0` `:561` | `chance = 50` `:239` |
| `OrbitalAttack` `cooldown_turns` | `1.0` `:567` | `cooldownTurns = 1` `:242` |
| `InterceptAttempt` `chance` | `50.0` `:589` | `chance = 50` `:253` |

Production JSON already spells these out (`config/tile_yield_rules.json`, `config/buildings/buildings.json` ASAT). `InterceptAttempt::cooldownTurns = -1` is a **sentinel** (“no deploy cooldown”), not a SMAC balance number — keep when key omitted. `chance_of_destruction_on_fail` default `0` is an optional switch — keep.

### `amount_source` / `EffectiveStatModifierAmount`

Parser restricts `amount_source` to energy + `ThisTile` (`413:423`) but **not** to `op: Add`. Resolve without a tile returns `0.0` (`96:100:include/game/effects/ActiveEffect.h`). Neutral for `Add` (pinned `ModifierMathTests.cpp:222`); for `MultiplyGeometric` it multiplies the stat by zero. Fix at parse time: require `op: Add` when `amount_source` is set.

### Council proposal shapes

`CouncilProposalConfigParser.cpp:75-76` calls validating `ParseEffects(..., CouncilProposal, id)`. `ValidateScopeForSource` only rejects `ThisPop`/`ThisUnit` on wrong sources (`668:680:BonusEffectParser.cpp`). Runtime keeps/applies:

| Persistence | Scope / type | Consumer |
|-------------|--------------|----------|
| Continuous | `WorldGlobal` (any type) | `CouncilEffects::RebuildWorld` via `IsContinuousWorldEffect` (`11:14`, `30:33:CouncilEffects.cpp`) |
| Instantaneous | `GrantEnergy` (scope ignored by applier) | `CouncilOutcomeApplier::ApplyInstantaneousEffects` `:29-34` |
| Instantaneous | `WorldParameter` | same applier `:36-41` — **documented TODO**, no map mutation |

Stock `config/council/proposals.json` uses Continuous `WorldGlobal` RuleFlag/StatModifier, Instantaneous `GrantEnergy` (`FactionGlobal`), and Instantaneous `WorldParameter` (solar shade / polar caps). A Continuous/`FactionGlobal` or Instantaneous/`StatModifier` proposal would load, pass a vote, and do nothing.

### Governor shapes

`CouncilRulesConfigParser.cpp:44-51` wraps `governor_effects` and parses with `EffectSourceKind_t::CouncilProposal` (wrong kind label) and only `ValidateScopeForSource`. Runtime:

- Continuous + `FactionGlobal` retained (`CouncilEffects::SetGovernorEffects` `:44-48`)
- Instantaneous infiltration applied (`CouncilOutcomeApplier::ApplyGovernor` → `ApplyInfiltrationEffects` `:45-52`)

Stock `config/council/rules.json` matches Continuous + `FactionGlobal` (StatModifier + Infiltration).

### `ValidateScopeForSource` too weak

Only `ThisPop`≠PopType and `ThisUnit`≠UnitComponent. Architecture (`effects-system.md:121-130`) intentionally leaves some combos legal-but-inert (e.g. FactionGlobal on improvements pending territory). But `ThisBase` / `ProducedAtThisBase` need an origin base: unit collectors never tag one (`CollectUnitEffects` uses `nullptr` origin — `ActiveEffect.cpp:351`). A `ThisBase` effect on a unit component validates, then is unused by unit/tile/faction-lane filters — silent no-op. Review example confirmed at `BonusEffectParser.cpp:668-680`.

### Tile yield rules skip validation

```12:21:src/game/effects/TileYieldRulesConfigParser.cpp
std::vector<EffectConfig_t> TileYieldRulesConfigParser::ParseConfig(const std::string& configPath)
{
    ...
    return BonusEffectParser::ParseEffects(json);
}
```

Non-validating overload; no `EffectSourceKind_t` for tile-yield rules (`BonusEffect.h:410-421` ends at `ProbeAction`). Caps in stock config use legal FactionGlobal scopes, so the gap is latent.

### Adjacent hygiene (mostly Pass 2)

- Hand-rolled `ParseModifierOp` / `ParseEffectScope` / `ParseEffectPersistence` despite `magic_enum` already used (`92:127`)
- `ParseNumber` + `std::stod` accepts `"2abc"` (`130:139`)
- `"effects"` iterated without `is_array()` (`686:690`)
- No JSON map for `difficult_terrain_cost` though `StatId_t::DifficultTerrainCost` exists (`11:43` vs `EffectEnums.h`)
- Local named `override` (`513`)

---

## Design decision

### Confirm: two passes, do not merge

| Pass | Theme | Why separate |
|------|--------|--------------|
| **1 — Strictness** | Fail loud: `.at()`, required balance keys, honored-shape validators, tighter scope matrix, tile-yield source kind, `amount_source`→Add, `radius` only on `ThisTile`, `effects` must be array | Behavior/requirement change for mods and tests; small diffs; shippable alone |
| **2 — Dispatch** | Per-type parse fns + map; shared `RequireScope_` / tile-stat helpers; `magic_enum` wire maps; `ParseNumber` hygiene; `difficult_terrain_cost` mapping | Large mechanical refactor; must not change accepted JSON semantics vs Pass 1 |

Package text and cross-package sketch already require this split. **Do not combine** unless Pass 2 is blocked on Pass 1 helpers — even then, land Pass 1 first.

### Confirm (with amendments)

1. **`.at("type")` / `.at("scope")`** — confirm. Prefer catching `nlohmann::json::out_of_range` and rethrowing `std::runtime_error` with key name if other parsers do that; otherwise raw `.at()` is enough if tests assert throw.
2. **Require balance keys** — confirm for `max`, OrbitalAttack `chance`+`cooldown_turns`, InterceptAttempt `chance`. Keep defaults for `chance_of_destruction_on_fail`, `apply_after_restriction`, `persistence`=`Continuous`, `radius`=`0`, InterceptAttempt omitted `cooldown_turns` → `-1`, `amount_source` scale omitted → `1.0` (identity).
3. **Drop conflicting struct balance defaults** — remove `TileResourceCapEffect_t::max = 2`, `OrbitalAttackEffect_t::chance/cooldownTurns` SMAC defaults, `InterceptAttemptEffect_t::chance = 50`. **Keep** `InterceptAttemptEffect_t::cooldownTurns = -1` and `chanceOfDestructionOnFail = 0`. Hand-built test structs must set chance/max explicitly.
4. **`amount_source` ⇒ `op: Add`** — confirm at parse; leave resolve `0.0` without tile (still correct for Add).
5. **Council / governor honored-shape validators** — confirm; place in council parsers (or shared helpers next to them), not inside `ValidateScopeForSource` (source-kind matrix ≠ runtime allowlist).
6. **Tighten `ValidateScopeForSource`** — confirm for certainly-impossible origin scopes; **do not** outlaw intentional legal-but-inert FactionWide-on-Improvement.
7. **Tile yield** — add `EffectSourceKind_t::TileYieldRules`; call validating `ParseEffects`. Optionally constrain that source to TileResourceCap + allowed faction-wide scopes (stock file is only caps).

### Amend: proposal `WorldParameter`

Review “pair” (Continuous+WorldGlobal, Instantaneous+GrantEnergy) would **reject** stock solar-shade / polar-caps proposals. Those are deliberate unimplemented stubs (`CouncilOutcomeApplier` TODO; `council-system.md` “council never mutates the world map”).

**Allowlist for proposals:**

1. `Continuous` + `WorldGlobal` (any effect type the continuous world store can hold)
2. `Instantaneous` + `GrantEnergy` (any scope; applier ignores scope and grants all members)
3. `Instantaneous` + `WorldParameter` + `WorldGlobal` — **explicit deferred** shape; document in validator comment / `council-system.md` that load is allowed, apply is no-op until WorldEvents

Reject everything else (e.g. Continuous FactionGlobal, Instantaneous StatModifier, ThisBase).

**Governor allowlist:**

1. `Continuous` + `FactionGlobal`
2. `Instantaneous` + `Infiltration` with scopes already required by Infiltration parse (`FactionGlobal` / `WorldGlobal`)

Use a dedicated source kind for governor parse (`CouncilRules` or `CouncilGovernor`) instead of lying with `CouncilProposal`.

### Rejected alternatives

| Alternative | Why not |
|-------------|---------|
| Merge Pass 1+2 | Violates package sequencing; review risk; hard to bisect |
| Reject `WorldParameter` at load / empty those proposal effects | Deletes documented future surface; stock + fixtures break for a stub the architecture already owns |
| Skip inert shapes via runtime warnings | Mods need load-time failure; runtime drop is the bug |
| Default `EffectConfig_t::scope`/`persistence` in-struct | Separate issue (indeterminate members); don’t paper over with defaults in this package |
| Broad “every scope must be collectible today” matrix | Contradicts documented legal-but-inert improvement faction lanes |

### Package interactions

- **Package 7** (reference validators): new `EffectSourceKind` / effect types must remain visit-covered; prefer landing Pass 1 before or with 7.
- **Package 3** (ActiveEffect contracts): null `config`, council pointer lifetime — out of scope here.
- **Package 8**: Condition/filter variants, `BonusEffect.h` vs `EffectEnums.h` rename — out of scope; light Pass 2 hygiene only as listed.

---

## Implementation plan

### Pass 1 — Strictness (ship first)

1. **`BonusEffectParser::ParseEffectConfig`**
   - `effectJson.at("type")`, `at("scope")`.
   - If `radius != 0` and `scope != ThisTile`, throw.
   - `StatModifier` + `amount_source`: require `op == Add` (default op is Add today — if `op` omitted, OK; if explicitly non-Add, throw).
   - `TileResourceCap`: require `"max"` (no default).
   - `OrbitalAttack`: require `"chance"` and `"cooldown_turns"`.
   - `InterceptAttempt`: require `"chance"`; `cooldown_turns` optional → `-1`.
   - Add `RequireNumber(parameters, key)` (throw if absent) vs optional `ParseNumber` with default.
2. **`BonusEffect.h`**
   - Remove balance member initializers listed above; keep InterceptAttempt cooldown sentinel and destruction chance `0`.
3. **`ParseEffects`**
   - If `"effects"` present, require `is_array()`; else throw.
4. **`ValidateScopeForSource`**
   - Keep ThisPop / ThisUnit rules.
   - Reject `ThisBase` and `ProducedAtThisBase` unless source is one that can supply `originBase` / pop-merge: at minimum **allow** `Building`, `PopType`, `SocialPolicy`, `SocialRating`; **reject** on `UnitComponent`, `Improvement`, `ProbeAction`, `Faction`, `CouncilProposal`, `CouncilRules`/`CouncilGovernor`, `TileYieldRules`.
   - Update header comment + `effects-system.md` “certainly-impossible” list; keep legal-but-inert FactionWide-on-Improvement.
5. **`EffectSourceKind_t`**
   - Add `TileYieldRules` and `CouncilRules` (or `CouncilGovernor`).
6. **`TileYieldRulesConfigParser`**
   - `ParseEffects(json, TileYieldRules, "tile_yield_rules")` (or path basename).
7. **Council honored shapes**
   - `ValidateProposalEffectHonored_(const EffectConfig_t&, id)` after parse in `CouncilProposalConfigParser`.
   - `ValidateGovernorEffectHonored_(...)` in `CouncilRulesConfigParser`; parse with new `CouncilRules` kind.
8. **Docs**
   - `docs/architecture/effects-system.md` — required keys, radius rule, expanded ValidateScope matrix, tile-yield source kind.
   - `docs/architecture/council-system.md` — honored proposal/governor shapes; WorldParameter deferred allowlist.
9. **Tests** — see Test plan Pass 1. Update any test that relied on omitted `max`/`chance`/`cooldown_turns` or pinned “only ThisPop/ThisUnit” validation.

### Pass 2 — Dispatch refactor (after Pass 1 green)

1. Extract `ParseGrantBuilding_`, `ParseStatModifier_`, … each taking `(parameters json, EffectConfig_t&)` or returning `EffectVariant_t`.
2. `unordered_map` / static table `type string → parse fn`; unknown type throws (same message).
3. Shared `RequireScope_(EffectScope_t, initializer_list / span)`, `IsTileResourceStat_(StatId_t)`.
4. Replace hand-rolled op/scope/persistence (and ideally amount_source) with `magic_enum::enum_cast` matching enumerator names.
5. Harden `ParseNumber`: reject trailing garbage; include key in `invalid_argument` wrapper.
6. Map `"difficult_terrain_cost"` → `DifficultTerrainCost`; extend ParserTests.
7. Rename `override` local; no drive-by r-prefix renames across the file (Package 8).

Pass 2 must not change which JSON documents load after Pass 1.

---

## Test plan

Requirement-based; update tests that pinned weaker requirements.

### Pass 1

| Requirement | Test locus |
|-------------|------------|
| Missing `type` or `scope` throws (no abort) | `ParserTests` new cases |
| Omitting `max` / OrbitalAttack `chance` or `cooldown_turns` / InterceptAttempt `chance` throws | `ParserTests` + existing orbital parse test if it omitted keys |
| `amount_source` with explicit non-Add `op` throws; Add (or omitted op) OK | `ParserTests` amount_source section |
| `radius > 0` on non-`ThisTile` throws; `ThisTile`+radius OK | `ParserTests` |
| Non-array `"effects"` throws | `ParserTests` |
| `ThisBase` / `ProducedAtThisBase` on `UnitComponent` throws; still OK on `Building`; FactionGlobal on Improvement still OK | Replace/extend `ValidateScopeForSource: rejects only the certainly-impossible…` |
| Proposal Continuous FactionGlobal / Instantaneous StatModifier throws; Continuous WorldGlobal + Instantaneous GrantEnergy + Instantaneous WorldParameter OK | New council parser tests (or extend `PlanetaryCouncilTests`) |
| Governor Continuous WorldGlobal / Instantaneous StatModifier throws; stock Continuous FactionGlobal OK | Council rules parser tests |
| Tile yield rules with illegal scope (e.g. `ThisPop`) throws | Small fixture or inline JSON through `TileYieldRulesConfigParser` |
| Stock `config/council/*.json`, `tile_yield_rules.json`, ASAT buildings still load | Existing load / council / orbital tests |
| Hand-constructed `OrbitalAttackEffect_t` / caps in unit tests set fields explicitly | Compile fix after dropping defaults |

### Pass 2

| Requirement | Test locus |
|-------------|------------|
| All existing Pass 1 + ParserTests remain green (semantics unchanged) | `./bd test` effects + council filters |
| `difficult_terrain_cost` parses; unknown still throws | `ParserTests` ParseStatId |
| Numeric string with trailing junk throws with key name | `ParseNumber` tests |

---

## AI implementation prompt

Self-contained. Run **Pass 1**, stop for review, then **Pass 2** in a follow-up (or second commit). Do not merge passes.

---

### Prompt A — Pass 1: Parser & shape strictness

You are implementing **Package 5 Pass 1** for the Alpha Centauri C++ rebuild at `/home/martok/alpha-centauri`: load-time effect JSON must fail loudly for illegal/unsupported shapes; no silent SMAC defaults; no abort on missing keys.

#### Goals

1. Required effect keys via `.at()`; balance parameters required or throw.
2. Drop duplicate balance defaults from effect structs (keep InterceptAttempt cooldown sentinel).
3. Restrict `amount_source` to `Add`; restrict nonzero `radius` to `ThisTile`.
4. Honor-shape validation for council proposals and governor effects (allow deferred `WorldParameter`).
5. Tighten `ValidateScopeForSource`; validate tile-yield rules with a new source kind.
6. `"effects"` must be a JSON array when present.

#### Constraints

- Follow `.cursor/rules/coding-guidelines.md` (throw over silent defaults; no back-compat shims).
- Build/test only via `./bd` (never raw cmake/make/ctest).
- Do **not** refactor `ParseEffectConfig` into a dispatch table (that is Pass 2).
- Do **not** implement WorldEvents / apply `WorldParameter`; only allowlist it for proposals.
- Do **not** outlaw FactionGlobal-on-Improvement (legal-but-inert pending territory).
- Keep architecture docs current: `docs/architecture/effects-system.md`, `council-system.md`.
- Prefer config numbers in JSON; C++ must not invent `2`/`50`/`1` for omitted balance keys.

#### Files to touch

- `src/game/effects/BonusEffectParser.cpp`, `include/game/effects/BonusEffectParser.h`
- `include/game/effects/BonusEffect.h` (`EffectSourceKind_t`, struct defaults)
- `src/game/effects/TileYieldRulesConfigParser.cpp` (+ header if needed)
- `src/game/council/CouncilProposalConfigParser.cpp`, `CouncilRulesConfigParser.cpp` (+ headers if exporting helpers)
- `tests/effects/ParserTests.cpp`, `tests/game/PlanetaryCouncilTests.cpp` and/or new council parser tests, orbital parser tests as needed
- `docs/architecture/effects-system.md`, `docs/architecture/council-system.md`
- Stock/fixture JSON only if something currently omits a now-required key (audit `config/` + `tests/fixtures/`)

#### Concrete rules

**ParseEffectConfig**

- `type` / `scope`: `.at(...)`.
- `radius != 0` ⇒ `scope == ThisTile` or throw.
- `TileResourceCap`: `"max"` required.
- `OrbitalAttack`: `"chance"` and `"cooldown_turns"` required.
- `InterceptAttempt`: `"chance"` required; omitted `cooldown_turns` ⇒ `-1`; keep optional `chance_of_destruction_on_fail` default 0.
- `StatModifier` + `amount_source`: throw unless op is Add (explicit or default).
- Introduce `RequireNumber` (or equivalent) for required numeric params; keep defaulting `ParseNumber` only for optional keys.

**Structs (`BonusEffect.h`)**

- Remove: `TileResourceCapEffect_t::max = 2`, `OrbitalAttackEffect_t::chance = 50`, `cooldownTurns = 1`, `InterceptAttemptEffect_t::chance = 50`.
- Keep: `InterceptAttemptEffect_t::cooldownTurns = -1`, `chanceOfDestructionOnFail = 0`.

**ParseEffects**

- If container contains `"effects"` and it is not an array → throw.

**ValidateScopeForSource**

- Existing ThisPop / ThisUnit checks.
- Reject `ThisBase` and `ProducedAtThisBase` on sources that cannot attach origin / pop-merge — reject at least on `UnitComponent`, `Improvement`, `ProbeAction`, `Faction`, council kinds, `TileYieldRules`.
- Allow on `Building`, `PopType`, `SocialPolicy`, `SocialRating`.
- Do not add rejections for FactionWide scopes on `Improvement`.

**EffectSourceKind_t**

- Add `TileYieldRules` and `CouncilRules` (name may be `CouncilGovernor` if clearer).
- Tile yield parser: validating `ParseEffects(..., TileYieldRules, ...)`.
- Governor parser: use `CouncilRules` kind (stop passing `CouncilProposal`).

**Proposal honored shapes** (throw with proposal id in message)

1. Continuous + WorldGlobal  
2. Instantaneous + GrantEnergy (any scope)  
3. Instantaneous + WorldParameter + WorldGlobal (deferred no-op apply — document)

**Governor honored shapes**

1. Continuous + FactionGlobal  
2. Instantaneous + Infiltration (scopes already enforced by Infiltration parse)

#### Acceptance criteria

- [ ] Missing `type`/`scope` throws `std::runtime_error` (or wrapped), does not abort.
- [ ] Omitting listed balance keys throws; stock configs still load.
- [ ] `ThisBase` on unit component fails validation tests.
- [ ] Bad proposal/governor shapes fail at parse; stock council JSON loads.
- [ ] Tile-yield path uses validating overload.
- [ ] Docs updated; `./bd test` relevant filters green (`[effects][parser]`, council, orbital as applicable).
- [ ] No Pass 2 dispatch-table refactor in this change.

#### Out of scope

Dispatch map, magic_enum migration for op/scope, `difficult_terrain_cost` wire map, ActiveEffect null-config contracts, WorldEvents implementation, package 7 validator exhaustiveness.

---

### Prompt B — Pass 2: ParseEffectConfig dispatch refactor

You are implementing **Package 5 Pass 2** after Pass 1 has landed on `/home/martok/alpha-centauri`. Restructure effect parsing for maintainability **without** changing which JSON is accepted.

#### Goals

1. Replace the `ParseEffectConfig` if/else chain with per-type parse functions + a dispatch table.
2. Shared helpers: `RequireScope_`, `IsTileResourceStat_` (or equivalent names matching project style).
3. `magic_enum` for ModifierOp / EffectScope / EffectPersistence (and AmountSource if clean).
4. `ParseNumber` rejects trailing garbage; errors name the key.
5. Wire `"difficult_terrain_cost"` → `StatId_t::DifficultTerrainCost`.
6. Rename `SocialEngineeringOverride` local `override`.

#### Constraints

- Pass 1 semantics are the contract — no new silent defaults; no loosening honored shapes.
- `./bd` only for build/test.
- No unrelated r-prefix / file-rename churn (Package 8).
- Update `effects-system.md` only if the “how to add an effect type” path changes (document: add enum alternative + parse function + table entry + tests).

#### Files

- Primarily `BonusEffectParser.cpp` / `.h`
- `ParserTests.cpp` (difficult_terrain_cost, stod hygiene)
- Architecture doc touch if extension steps change

#### Acceptance criteria

- [ ] Each effect `type` has one focused parse function; unknown type still throws clearly.
- [ ] Tile-resource and scope checks go through shared helpers (no triple-duplicated predicates).
- [ ] Full Pass 1 test suite + existing parser/council/orbital tests green with **no** intentional requirement changes.
- [ ] `difficult_terrain_cost` round-trips in ParseStatId tests.

#### Out of scope

Further ValidateScope matrix expansion, council runtime changes, Condition_t→variant (Package 8), reference validators (Package 7).
