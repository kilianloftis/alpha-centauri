# Package 12 — Population rules and calculators

Findings re-verified against the tree at `c972f91`.

## Verified diagnoses

### [H] `ForceRiot` and `Update` disagree on what keeps a riot alive

`RiotCalculator.cpp:21-41` — `ForceRiot` sets `m_bRioting`; `Update` clears it whenever
`ComputeCondition_` is false. Verified: `ProbeActionEffects.cpp` calls `ForceRiot` without
changing composition, so an incited riot survives only until the next `Update` on any base that
does not independently satisfy drones > talents. `IsRioting` is already read by `MoraleCalculator`
and `ProbeRules`, so the state is load-bearing.

The review's note that `PopulationManager::CheckRiotEndOfTurn` has no caller is **stale**:
`src/game/stages/Population.cpp:26` calls it every turn for every base. So this is not latent —
every incited riot is already being undone on the turn it is incited, unless the base happened to
be drone-majority anyway.

**Chosen:** two separate pieces of state — the natural condition (`drones > talents`, recomputed
every `Update`) and a forced riot with an explicit expiry turn. A base is rioting if either holds.
`Update` takes the current turn and retires an expired forced riot; it can no longer clear one
that has not expired.

**Deferred, with reason:** how long an incited riot lasts is a game rule I do not have. The probe
action config carries the duration (`riot_turns`), defaulting to 1 turn with a TODO — one turn is
the minimum that makes the feature do anything at all, and the value is data, not code, so
correcting it later is a config edit.

### [M] `ResolveCurrentType` can hang on an obsolescence cycle — **no longer reproduces**

`PopTypeAvailabilityCalculator.cpp:65-94` already carries a `visited` set and throws naming the
cycle. Closed by an earlier package; no work here.

### [M] Availability obsolescence is one level deep and order-tied

`PopTypeAvailabilityCalculator.cpp:41-56` — verified. `GetAvailable` collects `obsoletes` ids only
from types that are *themselves* currently available. `config/pop_types.json`: `Transcend`
obsoletes `Empath` and `Thinker`; `Empath` obsoletes `Doctor`. With Transcend's tech but not
Empath's, nothing claims the `Doctor` edge, so Doctor stays assignable beside Transcend — and
`ResolveCurrentType("Doctor")` also stays on Doctor, because it gates each step on that step's
own tech.

The deeper problem is that the two methods answer the same question by different walks, so they
can disagree. That is what let the gap exist.

**Chosen:** one reachability helper both methods call. `obsoletes` edges are followed
transitively and *without* gating intermediate steps on their tech — Transcend obsoleting Empath
which obsoletes Doctor means Transcend obsoletes Doctor, whether or not Empath was ever
researched. `GetAvailable` drops a type iff some available type reaches it; `ResolveCurrentType`
returns the furthest available type reachable from the start. The two are then consistent by
construction: `GetAvailable` excludes X exactly when `ResolveCurrentType(X) != X`.

Ties (two available successors at equal distance) resolve by registry order, documented at the
helper rather than left implicit. **Rejected:** throwing on a tie. It is a plausible mod shape
(two specialist branches from one root), and a runtime throw on ordinary data is the failure mode
the last three packages kept introducing.

### [M] Composition calculator floors negative results to zero

`PopCompositionCalculator.cpp:32-33` — verified. Now that `EvalInt` throws rather than returning
0 on a broken formula (package 11), a negative result can only come from a formula that genuinely
computed one, so it is a config error and throws with the formula and the value.

### [M] `IsSpecialist` does not exclude talents; role is inferred from contribution magnitude

`Pop.cpp:32-45` — verified. `IsDrone` is `riotContribution > 0`, `IsTalent` is
`goldenAgeContribution > 0`, `IsSpecialist` is `!bCanWorkTile && riotContribution == 0`. A
non-worker with a golden-age contribution is both specialist and talent, and both counters count
it. Shipping data keeps talents as workers, so it is latent.

**Chosen:** an explicit `role` on the pop type — `worker`, `drone`, `talent`, `specialist` — so
the four predicates partition the set by construction.

`riot_contribution` and `golden_age_contribution` are **deleted** rather than kept as magnitudes.
Once role is declared, nothing reads them: the riot rule compares drone and talent *counts*, and
the golden-age rule compares counts too. Keeping them would leave two inert config keys and two
dead accessors, which the guidelines forbid. If a weighted rule appears later it can be added
with its consumer.

`role` is required config, per package 11's precedent, and `PopTypeRegistry::Validate_` rejects
the combinations that cannot mean anything: a `specialist` that can work a tile, or a
`worker`/`drone`/`talent` that cannot.

### [M] No direct tests for three of five calculators

Verified: no test file mentions `RiotCalculator`, `GoldenAgeCalculator` or
`PopTypeAvailabilityCalculator`. Both bugs above are calculator-level and would have been caught
by a handful of direct cases.

## Review follow-ups

The review confirmed the riot counter's arithmetic, the load-time DFS, and that no predicate
changes its answer for any shipping or fixture pop type. What it found:

1. **The contribution-magnitude coupling survived in the UI.** `PopulationDisplay::PopGroupOrder`
   still grouped by `GetGoldenAgeContribution()` while the glyph four lines below used
   `IsTalent()` — two definitions of "talent" in one function, which is the finding restated.
   Both now use roles, and the two accessors and config fields are deleted (above).

2. **The documented tie-break was false.** `resolvedId` was last-write over
   (frontier order × registry order), so a tie at depth 1 went to the *later* registry entry and
   a tie at depth 2 to the *earlier* one — unpredictable, in exactly the two-branch mod shape the
   design says it accepts. Now an explicit per-level minimum by registry index, and the header
   states the deeper-beats-shallower rule too.

3. **The "cannot disagree" test asserted only one direction**, inside an `if`, so a `GetAvailable`
   returning nothing would have passed it. Now asserts set equality with an independently
   computed expectation, plus a non-empty guard.

4. **Two behaviours shipped untested** (the composition negative-target throw and the growth
   saturation guard), and the new required `role` key had no strictness test at all — the thing
   Package 11 established a case per required field for.

5. `riot_turns` defaulted silently in the same change that made `role` required. It is now
   required on `incite_drone_riots` and rejected on every other action, so a modder cannot set it
   somewhere it is ignored.

6. Smaller: a header promising a throw that cycle-detection-at-load made impossible, an
   overstated `GetAvailable` invariant, an error message printing `Specialist` where the config
   says `specialist`, a golden-age test whose last block asserted from an already-false state, a
   deleted processor-reset assertion restored, `TempConfigFile` shared instead of duplicated, and
   the tech set built once per `GetAvailable` rather than once per candidate.

## Deferred from this package

- The real SMAC duration of an incited riot (above).
- A successor that is not `player_assignable` now supersedes its predecessor, where the old
  one-level walk only considered assignable types. Consistent with the new invariant, but it
  means a mod can convert pops into a type the player cannot pick. Left as-is: forbidding it is a
  rule decision, and the alternative (ignoring non-assignable successors) silently reintroduces
  the disagreement between the two methods.
