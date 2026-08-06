## Planetary Council — configuration and registries

**Files:** `src/game/council/CouncilProposalConfigParser.cpp`,
`include/game/council/CouncilProposalConfigParser.h`,
`include/game/council/CouncilProposalConfig.h`,
`include/game/council/CouncilProposalRegistry.h`,
`src/game/council/CouncilRulesConfigParser.cpp`,
`include/game/council/CouncilRulesConfigParser.h`,
`include/game/council/CouncilRulesConfig.h`

**Assessment:** Both parsers are small and readable, they throw on unknown enum strings rather
than defaulting, and the registry adds genuine cross-reference validation for
`required_proposals` / `repeals`. Cross-cutting references are also covered elsewhere:
`required_tech` by `RequiredTechValidator.cpp:78` and effect-borne ids by
`EffectReferenceValidator.cpp:194`, so those are not gaps. The dominant weakness is that the
proposal schema is a permissive *superset* of what the runtime honors — several field
combinations load cleanly and then do nothing at all, which is the one failure mode a
data-driven, moddable proposal list cannot afford. Adding a proposal built from existing
effect types is pure data (good); adding a new *outcome* still requires C++.

### [H] Proposal `effects` are accepted in shapes the council can never apply
`src/game/council/CouncilProposalConfigParser.cpp:75` hands the effect array to
`BonusEffectParser::ParseEffects`, whose only source check (`ValidateScopeForSource`) rejects
`ThisPop`/`ThisUnit` and is deliberately permissive about everything else. But the council
consumes proposal effects in exactly two places: `CouncilEffects.cpp:30` keeps only
`Continuous` + `WorldGlobal`, and `CouncilOutcomeApplier.cpp:29` applies only `Instantaneous`
`GrantEnergy` (the `WorldParameter` branch is a documented TODO). So a proposal carrying a
`Continuous`/`FactionGlobal` bonus, an `Instantaneous` `StatModifier`, or a `ThisBase` effect
parses, validates, ships, passes a vote — and silently does nothing. A modder's only signal is
that the game does not change. Fix: give the council parser its own honored-shape check
(mirroring `ValidateScopeForSource`) that rejects any proposal effect outside the pair the
runtime implements, so unsupported combinations fail loudly at load.

### [M] `kind` and `election_outcome` are parsed independently, and mismatches half-apply
`CouncilProposalConfigParser.cpp:53` and `:70-73` parse the two fields with no consistency
check. A `"kind": "standard"` proposal with `"election_outcome": "planetary_governor"` passes
its vote, records the pass, applies instantaneous effects — and is then neither activated
(`PlanetaryCouncil.cpp:532` skips activation for governor/supreme-leader outcomes) nor able to
install a governor (`:541` needs an election winner, which a standard ballot never produces).
The result is a proposal that reports `Passed` and changes nothing. The inverse — `election`
with no outcome — computes a winner in `TallyElection_` and discards it. Fix: reject the
mismatch at parse time (`electionOutcome != None` implies `kind == Election`), and state
explicitly in `CouncilProposalConfig.h` what an outcome-less election means.

### [M] `vote_threshold` is validated for every proposal but only honored by elections
`CouncilProposalConfigParser.cpp:55-60` accepts and range-checks `vote_threshold` regardless of
`kind`, which reads as a promise that it works. `TallyStandard_` (`PlanetaryCouncil.cpp:452-474`)
never reads it — a standard proposal always resolves on `yea > nay`, so a config author asking
for a two-thirds standard vote gets a simple majority with no warning. The field comment
compounds this: `CouncilProposalConfig.h:39-41` says `0 = simple majority of non-abstaining
weight (Yea > Nay)`, but on the only path that reads the field (`PlanetaryCouncil.cpp:505-517`)
`0` means *plurality* — any candidate with a single vote wins. Fix: reject `vote_threshold` on
`Standard` proposals until the tally honors it, and correct the comment to describe the
election semantics.

### [M] `vote_threshold` is a `double` where the project has an exact rational type
`CouncilProposalConfig.h:41` stores the threshold as `double` and
`CouncilProposalConfigParser.cpp:55` reads it as a JSON number, so the only way to express a
two-thirds supermajority is `0.6666...`. `PlanetaryCouncil.cpp:513-515` then compares it against
`bestVotes / totalWeight` computed in floating point, making the boundary case decided by
rounding. `lib/Rational.h` exists for precisely this ("config values that may be ints (2) or
fraction strings (\"1/3\")") and is already used by `ImprovementConfigParser.cpp:209`. Fix:
parse `vote_threshold` with `Rational_t::ParseJson` and have the tally compare cross-multiplied
integers.

### [M] `governor_effects` are parsed without checking the shapes the runtime keeps
`CouncilRulesConfigParser.cpp:44-51` parses the governor effect list with no scope/persistence
validation, but `CouncilEffects::SetGovernorEffects` (`CouncilEffects.cpp:44`) retains only
`Continuous` + `FactionGlobal` entries and `CouncilOutcomeApplier::ApplyGovernor` handles only
infiltration. A governor bonus written with the wrong scope is dropped without a word — the same
class of failure as the proposal effects above, on the config surface the architecture doc calls
out as "fully config-driven". Fix: validate the honored shapes in this parser.

### [M] Interval defaults live in two places and a misspelled key falls back silently
`CouncilRulesConfigParser.cpp:28-31` reads both intervals via `json.value(key, <struct default>)`,
so `governor_propose_interval_years` misspelled in `rules.json` yields `10` from
`CouncilRulesConfig.h:14` with no diagnostic, and the shipped `config/council/rules.json` repeats
the same two numbers that the header already hardcodes — two sources of truth that can drift.
The project guideline is to prefer throwing over returning defaults. Fix: require both keys (and
drop the header initializers), or keep the defaults and reject unknown keys in the object.

### [M] `required_proposals` documents an invariant the runtime does not hold
`CouncilProposalConfig.h:49-50` says the listed ids "must currently be in force (active)", but
`PlanetaryCouncil.cpp:535` adds *every* passed non-election proposal to the active set and never
removes it unless something repeals it. So "active" means "in force **or** has ever passed", and
the two shipped uses want different things: `increase_solar_shade` → `launch_solar_shade` means
"has passed" (that proposal projects nothing continuous), while `repeal_trade_pact` →
`global_trade_pact` really does mean "in force". A modder reading the comment cannot predict
which they get. Fix: either state the actual semantics on this field, or split it into
`requires_in_force` / `requires_passed` and let the council answer each from the right store
(`m_activeProposalIds` vs `m_passCounts`).

### [L] Convention and hygiene items
- `include/game/council/CouncilProposalConfigParser.h:17-20` — private methods lack the required
  trailing underscore (`ParseProposalConfig`, `ParseVoteWeight`, `ParseKind`,
  `ParseElectionOutcome`), while the anonymous-namespace free function
  `CouncilProposalConfigParser.cpp:15` carries one it does not need.
- `src/game/council/CouncilProposalConfigParser.cpp:80-92` — `ParseVoteWeight` / `ParseKind` are
  hand-rolled string chains whose wire forms differ from the enumerators only by case
  (`"standard"` ↔ `Standard`); the guidelines call for `magic_enum::enum_cast` here, as
  `BonusEffectParser.cpp:502` already does. `ParseElectionOutcome` correctly keeps an explicit
  map (`"supreme_leader"` ↔ `SupremeLeaderVictory`).
- `src/game/council/CouncilProposalConfigParser.cpp:80-101` — all three helpers are stateless
  `const` members; moving them beside `ParseRuleFlagList_` in the anonymous namespace would
  shrink the header to the single `ParseConfig` the registry template requires.
- `src/game/council/CouncilRulesConfigParser.cpp:14-25` — open / parse / `is_object` boilerplate
  duplicated in at least four other single-object parsers (`MoraleConfigParser.cpp:58`,
  `BaseConquestConfigParser.cpp:12`, `TileYieldRulesConfigParser.cpp:14`,
  `GrowthConfigParser.cpp:13`); `lib/config/JsonConfigLoader.h` only covers top-level arrays and
  wants an object variant.
- `src/game/council/CouncilRulesConfigParser.cpp:47-48` — the `wrapper["effects"] = ...` trick to
  satisfy `ParseEffects`' container contract is duplicated at `ProbeActionConfigParser.cpp:60`; a
  `ParseEffectList(const json& array)` overload in `BonusEffectParser` would remove both.
- `src/game/council/CouncilRulesConfigParser.cpp:50` — governor effects are labelled
  `EffectSourceKind_t::CouncilProposal`; their source is the governorship, not a proposal. Only
  affects diagnostics today, but it is a wrong fact recorded in code.
- `src/game/council/CouncilProposalConfigParser.cpp:17-33` — `ParseRuleFlagList_` re-implements the
  string-array read that `ConfigFields::ParseStringArray` already provides; only the flag mapping
  is council-specific.
- `include/game/council/CouncilProposalConfig.h:59-68` — `IsAvailable` is the fourth verbatim copy
  of the same linear discovered-tech scan (`BuildingConfigParser.h:36`, `SocialPolicyConfig.h:29`,
  `PopTypeAvailabilityCalculator.cpp:29`).
- `src/game/council/CouncilProposalConfigParser.cpp:47`, `CouncilRulesConfigParser.cpp:12` —
  reference parameters `proposalJson` / `configPath` lack the `r` prefix used by `rJson` and
  `rValue` in the same files.

**Observed outside slice:**
- `tests/fixtures/council/proposals.json`, `tests/fixtures/council/rules.json` — byte-identical
  copies of the shipped `config/council/*` files; the suite will keep passing after the real
  config drifts away from them.
- `src/game/council/PlanetaryCouncil.cpp:535` — every passed non-election proposal is activated
  permanently, so `repeal_trade_pact` (not `repeatable`, no continuous effects) can be used once
  per game even though the pact it repeals can be re-enacted; same for `repeal_un_charter`.
- `src/game/council/CouncilOutcomeApplier.cpp:31-34` — `GrantEnergy` is paid to every member
  regardless of the effect's declared `scope` or any faction filter, so those fields are inert
  decoration on instantaneous proposal effects.
