# Game rules — decisions of record

Rules the review packages hit and deliberately did not invent. Each was left as a TODO at its
call site until the project owner ruled on it. Recorded here so the TODOs can be closed against a
written rule rather than a guess.

Decided 2026-08-08.

---

## 1. Which pop is lost when a base shrinks

**Rule:** pops the player deliberately chose are taken **last**. Within whichever group is
being drawn from — the rest first (which includes drones, talents and default workers), or
player-choice pops only if there is nothing else left — take the pop **producing the least total
resource**.

A pop is player-choice when its type is `player_assignable` and not `is_default`
(`Pop::IsPlayerChoiceType`). Default workers are assignable but unprotected: they are what
composition converts from.

"Total resource" is the sum of what that pop currently contributes: for a tile worker, the
nutrients + minerals + energy of its worked tile; for a specialist, its econ + labs + psych
output.

**Replaces:** `PopContainer::RemovePop`'s unconditional `pop_back`, which took the most recently
added pop and so could take a talent while drones remained.

**Applies to** every shrink path: starvation, conquest, probe pop-kill, `EnforceMaxSize_`.

**Implemented** 2026-08-08. `PopulationManager::SelectDoomedPop_` orders candidates by
`(IsPlayerChoiceType, value)` and takes the minimum, so any ordinary pop outranks any
player-choice pop and the lowest producer within a group goes first; ties keep the earliest pop, which makes the choice
deterministic rather than allocation-order dependent. The value itself is injected by
`BaseManager` (`SetPopValuator`) because only it can resolve a worked tile's yield — the same
shape as `WorkerAssignmentManager::SetTileScorer`. An unassigned worker scores zero, so idle pops
go before productive ones. `PopContainer` lost `RemovePop`/`NextRemoved` and gained
`Remove(Pop&)`: which pop dies is policy, and the container only owns storage.

## 2. A base that reaches size zero

**Rule:** it is razed, immediately, wherever it happened. There is to be **one** raze pathway —
not a second one written for each way a base can be emptied.

**Replaces:** the guard added in `BaseManager`'s starvation handler, which returned early at size
zero and left an empty base alive forever.

**Implemented** 2026-08-08, **revised** 2026-09-02. The pathway is now `BaseManager`'s pop-loss
handler: any pop loss that leaves the base at size zero calls `Faction::RazeBase`. Starvation, a
production pop cost, genetic plague, conquest and any future world event all reach it without
knowing about each other. Conquest's `NoteIfAlreadyRazed_` no longer razes — it only reports whether
the raze already happened.

Razing is split from destroying, which is what lets it run from inside the base's own signal
handler and mid-iteration. `Faction::RazeBase` does everything observable — tombstones secret
projects, orphans home-base claims, releases the tile, drops deploy records, emits
`OnDestroyed` — and marks the base razed; `Faction::Bases()` and `FindBase` skip razed bases from
that instant, so nothing in the game can still see it. The `BaseManager` object itself is
destroyed later by `Faction::ReapRazedBases`, which `TurnProcessor::Advance` calls between
stages. That timing carries no game meaning; it exists so references held across the raze do not
dangle.

## 3. Production retooling penalty

**Never implemented** — the review recorded it as "carried the full stockpile", which was
accurate; there is no penalty code anywhere in the tree.

**Rule:** when switching production, if **more than 10 minerals** have been spent toward the
current item, **50% of the spent minerals are lost**.

- Switching **back to the original** item incurs **no** penalty.
- Switching to a **third** item applies the penalty **again**.
- The "original" is tracked for the current turn at least; it need not survive a turn boundary.

**Implemented** 2026-08-08. `ApplyProduction` marks the item in place once that turn's minerals
are banked — the last thing to touch production before `PlayerActions` hands the player control —
and `SetProduction` charges the penalty when switching to anything else. Numbers live in
`config/production.json` (`retool_penalty_threshold`, `retool_penalty_percent`). Production
cost is `baseCost * CostMultiplier` (Industry via effects); SMAC's "minerals per row" is a UI
presentation of progress, not a cost factor, and is not configured here. The forfeit is scaled
by `RetoolPenaltyScale` from the base's effects (Skunkworks `MultiplyGeometric` 0 cancels it);
callers with a live base pass `GetBaseEffects()` into `SetProduction`.

Clearing production (`nullptr`) is not charged: the stockpile is kept. Re-queuing pays once a
turn original exists; while the turn original is still null (fresh base, or last
`ApplyProduction` had nothing queued), queue and switch stay free. Integer division rounds the
forfeit down, so an odd stockpile favours the player.

**Founding minerals:** `StartingMinerals` effects (colony pod, SE, secret projects, …) are
credited into the new base's production stockpile once at founding (`TryFoundBase` only —
never on transfer or snapshot restore). Retooling is free while the turn original is still
null (a fresh base has none); once `ApplyProduction` banks a turn with something queued and
stamps an original, ordinary retooling rules apply.

## 4. Faction elimination

**Rule:** factions are **never removed** from the game. A defeated faction's leader can be freed
to re-establish it, so the faction object must persist.

**Consequence:** `EvFactionElim` described an event that can never occur.

**Implemented** 2026-08-08. Removed from the mod-facing catalogue rather than left as a promise
the rules cannot keep. If "faction lost its last base" turns out to be worth publishing, that is
a different event with a different name.

## 5. Council election veto

**Rule:** governor rules do **not** apply to elections.

**Consequence:** `VetoUnanimouslyOverruled_` reading standard ballots during an election is not a
gap to fill — the veto/overrule path simply does not apply to elections.

**Implemented** 2026-08-08. `VetoPending` refuses an election outright, so the overrule check can
never see one; both sites say why. The office cannot be defended by its incumbent.

## 6. Psych / energy economy

**Rule:** energy is produced and allocated **per base**. There is no faction-wide energy pool
that is later redistributed.

Pipeline at each base:
1. Produce energy (tiles, crawlers, buildings, Energy StatModifiers — any source).
2. Apply inefficiency (distance from HQ + base efficiency rate from SE + local sources).
3. Split post-inefficiency energy into labs / econ / psych via the faction EconomyManager
   percentages (floored labs/psych; residual econ).
4. Apply Econ / Labs / Psych StatModifiers to each category.
5. Faction collects econ (treasury) and labs (research). Psych stays at the base and is
   consumed when calculating population composition.

Psych may also originate from facilities, secret projects, and specialists (Psych
StatModifiers), on top of the local energy-psych share.

**Consequence:** composition's `psych_output` counting specialist psych only is wrong — it is one
source among several.

**Implemented** 2026-08-09 (per-base pipeline). Inefficiency implemented
2026-08-09: tabletop-diagonal HQ distance + Efficiency SE rating
(`Inefficiency = Energy × Distance / denominator`, with per-level denominators in
`social_rating_effects.json`; no HQ → Distance 16; HQ base loses nothing; loss capped
at Energy; denom 0 → 100% loss).

**Psych is per-turn, never consumed** (2026-08-27). `ConsumePsych` became `GetPsych`, a
non-destructive read, and `ProduceResources` resets the bank. Two reasons: composition
recalculates several times a turn, so a draining read let the first pass empty the bank and
every later pass undo its own talents; and the player needs to preview what this turn's psych
will do to a base before committing, the same way minerals preview against production. The psych
ladder is now the only psych consumer — the old `talent_formula` spent the same psych a second
time.

## 7. Prototype StartingExperience — first one you built

**Rule:** `Unit::IsPrototype()` (and therefore the production.json prototype
`StartingExperience` bonus) applies only to a unit the faction **produced**. Free
spawns — Engine starting units, BaseConquestEffects escape pods, and any other
`CreateUnit` without an explicit production base — do **not** latch as prototypes
and do not collect the bonus.

**Ledger unchanged:** every `CreateUnit` still calls `Military::RecordBuiltComponents`,
so a free scout still unlocks those components for the mineral surcharge. The next
*built* copy is ordinary (no surcharge, no prototype XP).

**Signal:** `Unit`'s constructor latches prototype only when `pProducedAt != nullptr`
and `Military::IsPrototype(design)` is true at that moment. Production completion
passes the building base as `pProducedAt`; gift paths omit it.

**Replaces:** the Unit.cpp TODO that left "first of its kind" vs "first one you built"
undecided while free units collected the bonus.

**Implemented** 2026-08-12.

---

## 8. Hurrying production — open questions

**Implemented** 2026-08-15, with the shape of the mechanic settled and the numbers not. Energy
credits buy minerals into the queued item's stockpile; the price per item class is a Lua
expression in `config/production.json` `kinds.<kind>.hurry.formula`, keyed by
`IConstructable::GetConstructableKind()`. A kind with no hurry entry cannot be hurried, which is also how
stockpiles are excluded and how a mod turns the mechanic off.

Two things in it are **not** rules of record, and are marked TODO at the config:

- **The below-threshold surcharge.** Minerals still needed while the stockpile sits under
  that kind's `mineral_threshold` are billed `below_threshold_multiplier` times. Both numbers
  live on the kind so a facility and a unit need not share a band. The multiplier stands in
  for SMAC's doubling of the cheap early minerals; the mineral_threshold it triggers on is a
  guess. It is deliberately independent of `retool_penalty_threshold` — they happen to ship at
  the same value, but retooling and hurrying are unrelated mechanics.
- **Whether partial payment should exist at all.** SMAC hurries an item outright. This
  implementation lets the player buy any number of minerals they can afford. The *pricing* of a
  partial buy is settled and not a guess: buying `k` minerals costs what it takes off the finish
  price, so instalments always total the quoted price. That is the only model that holds under a
  formula which is not linear — pricing an instalment as a flat fraction of the finish cost let a
  player pay 41 credits for a 60-credit unit by buying a mineral at a time. If the rule turns out
  to be all-or-nothing, `HurryProductionCalculator::ApplyCredits` and `HurrySpend_t` come out and
  `QuoteHurry` is the whole API.

---

## 9. Riot and golden age range over the composition pool

**Rule:** neither riot nor golden age is a function of base size. Both sum per-pop weights over
the composition pool — drones, super drones, workers and talents — and specialists participate
in neither.

- Riot while `Σ riot_weight >= riot_threshold` (shipping threshold **1**).
- Golden age while the base has **no** drone-class pops **and**
  `Σ golden_age_weight >= golden_age_threshold` (shipping threshold **0**).

With Drone `+1` / Talent `−1` the riot rule is "at least one net drone". With Talent `+1` and
plain workers and drones `−1`, the golden age rule is `talents >= workers + drones`.

The thresholds differ deliberately: riot needs strict net unrest, golden age allows the tie.

**Changes behaviour.** `GoldenAgeCalculator` previously counted specialists on the non-talent
side, so a doctor-heavy base could not reach a golden age. It now can. Riot already ignored
specialists, so this makes the two consistent rather than introducing an asymmetry.

**Drone weight is not riot weight.** `StatId_t::Drones` is drone *pressure*; a Super Drone body
absorbs 2 of it (`drone_weight`) but riots at `+1` like any other citizen. The old
`riot_contribution: 2` conflated the two.

**Implemented** 2026-08-27. Weights are `ThisPop` effects on the pop types; thresholds are
`riot_threshold` / `golden_age_threshold` scalars in `pop_composition.json`. See
`docs/architecture/population-system.md`.

## Deferred by decision, not by uncertainty

- **Mid-proposal trade failure should crash.** A `TransferBaseTo` that throws part-way through a
  multi-item trade is a programming error, and the game should fail loudly. No rollback machinery
  is wanted. Current behaviour (the exception propagates) is therefore correct as it stands.
- **A diplomatic proposal queue is not needed.** Turns are sequential; one pending slot with a
  `Busy` refusal is sufficient until and unless multiplayer happens.
- **Player-action command/event seam:** wanted, but after the core game.
- **Lua mod scripting:** wanted, but after the core game.
- **No `[a-b of N]` overflow indicators anywhere.** Lists scroll; they do not annotate themselves
  with counts. This applies to existing indicators as well as new ones.
