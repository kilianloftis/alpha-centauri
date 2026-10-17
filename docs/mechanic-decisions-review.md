# Mechanic decisions review

**Date:** 2026-10-16
**Scope:** `src/game` at `96119e2`: state owners, mutations, executors, turn stages, calculators and every `*Rules` file. Headers were read with their implementations.
**Lens:** one mechanic, one function. When a player asks for a mechanic to change, the change should land in one Rules function, and that function should decide nothing else.
**Plan:** decisions found here move into rules in commits 7–13 of [`docs/plans/code-organisation.md`](plans/code-organisation.md). Findings that would change behaviour, and hard-coded numbers that belong in config, are collected in [Outside the plan](#outside-the-plan) for a separate package.

## Summary

Each finding carries one or more tags:

- `[D]` a decision a player would call a mechanic, made outside a Rules function
- `[M]` one function deciding more than one mechanic
- `[X]` the same decision made in two or more places
- `[N]` a hard-coded number a player could ask to change
- `[B]` borderline: your decision on each is recorded under [Decisions](#decisions)

| Domain | Findings | D | M | X | N | B |
|--------|---------:|--:|--:|--:|--:|--:|
| Population | 9 | 6 | 0 | 2 | 0 | 3 |
| Production and buildings | 14 | 8 | 1 | 3 | 2 | 3 |
| Economy | 8 | 5 | 1 | 1 | 2 | 2 |
| Research and social engineering | 4 | 4 | 0 | 0 | 0 | 0 |
| Ecology | 3 | 3 | 0 | 0 | 0 | 0 |
| Combat | 13 | 11 | 1 | 1 | 2 | 0 |
| Movement | 9 | 7 | 0 | 3 | 3 | 0 |
| Terraform and map | 7 | 5 | 0 | 0 | 2 | 1 |
| Diplomacy and atrocities | 6 | 6 | 0 | 0 | 0 | 0 |
| Base conquest | 4 | 4 | 0 | 0 | 2 | 0 |
| Probes | 6 | 4 | 0 | 1 | 2 | 1 |
| Planetary Council | 5 | 5 | 0 | 0 | 1 | 0 |
| Effects plumbing | 1 | 0 | 0 | 1 | 0 | 1 |
| **Total** | **89** | **68** | **3** | **12** | **16** | **11** |

Each finding counts under every tag it carries, so the tag columns add up to more than the findings column. Unconditional consequences are listed per section and are not counted.

Where they go:

- **Already in the plan:** 13 findings, which keep their commit-7 entries and move to their domain's commit.
- **Added to the plan:** 63 findings, in commit 2 and commits 7–13.
- **Outside the plan:** 8 findings live only there. The package holds 8 behaviour changes, some of them from findings that are also in the plan. The 18 hard-coded numbers go to the game-config expansion instead.
- **Stay as they are, by decision:** 5. Every decision is recorded under [Decisions](#decisions).

### What the review found

The organisation plan is about where each kind of code lives. This review adds two things it could not see from the headers.

First, the drift concentrates in a few places. `UnitOrderExecutor` makes eight decisions of its own: what an attack costs, who rolls for promotion, how a unit pays for a tile it cannot afford, how hard an airdrop lands, what a bombard offends and destroys, which units may found on water, and how fast a former works. `PlanetaryCouncil` holds every council decision as a method on the state owner: vote weight, candidates, tallies and the veto. `BaseManager` and `ResourceManager` decide most of the production and energy mechanics. The `map/` mutations (explosion, fungal bloom, elevation, spread, surface flips) choose their targets inside the code that applies them. The `*Rules` files themselves are in good shape: only one function decides two mechanics, `CollectBombardTargets`.

Second, three findings are about behaviour, not organisation:

- **The defender is whoever comes first on the tile.** `FindVisibleHostileOnTile` (`src/game/units/AttackRules.cpp:105`) returns the first visible hostile in tile order, and `TryAttack` fights it. Nothing picks the best defender. The artillery-duel opponent is chosen the same way.
- **A probe's displayed odds and its roll disagree.** `ComputeProbeChances` treats only negative risk as a sure thing. `RollProbeAction` also treats risk 0 as a sure thing, and most of `probe_actions.json` is risk 0. The Hunter-Seeker Algorithm blocks probe teams and halves success, so only a probe with Algorithmic Enhancement can act against its owner. For that probe, a risk-0 mission shows 50% and always succeeds.
- **Probe repeat risk depends on executor memory.** The record of which bases a faction has probed lives in `ProbeActionExecutor`. It is never cleared and is not saved, so repeat risk resets on load.

### Your examples, after the plan

- **Growth at the population limit keeps half the box.** That becomes `GrowthRules::ResolveGrowth` (already in the plan). A switch for the percentage reads `GrowthConfig_t` there.
- **Disabling collateral damage.** That becomes `CombatRules::PlanStackCollateral` (already in the plan).
- **Probe actions always trigger Vendetta.** Whether a probe act is attributed is `ProbeRules::ProbeHostileAct` today. Whether an attributed act causes Vendetta becomes `DiplomaticTransitionRules::VendettaFromHostileAct` (already in the plan). The "no Vendetta while in a pact" switch goes there too.
- **Disabling artillery duels.** This was not simple. `CollectBombardTargets` decides both whether a duel happens and which units a strike hits, so the switch would sit inside a function that also decides the strike targets. CMB-5 splits it, and the switch then goes in `AttackRules::ArtilleryDuelTarget`.

### One principle the plan now states

Some consequences have no condition, no choice and no amount: a transferred base resets its riot ladder; a failed probe is destroyed. These stay in the mutation that applies them. A Rules function that always returns the same answer would be speculative. When someone adds a switch to one of them, that change extracts the rule. Each section lists the consequences it leaves in place under "Stays with its mutation".

### Excluded

- **Player-side policy.** These decide as a player would, not as the rules do: worker auto-assignment and its tile scorer, `ResearchSelector`, `CouncilAiStub`, `DiplomaticActionExecutor::EvaluateResponse_`, the AI's deferral of abandoning completions in `BaseProduction`, `TryAssignAutoReturnToFuel`, stopping a move on new contact, and the player prompt on refused territory entry.
- **World generation.** It has its own config pipeline.
- **The effects language.** The triggered-effect handlers do what the effect config says. Their semantics belong to the effects system.
- **Engine invariants:** move fragments, id allocation, deferral scopes, and razing a base with no pops (POP-9).

## Contents

- [Population](#population)
- [Production and buildings](#production-and-buildings)
- [Economy](#economy)
- [Research and social engineering](#research-and-social-engineering)
- [Ecology](#ecology)
- [Combat](#combat)
- [Movement](#movement)
- [Terraform and map](#terraform-and-map)
- [Diplomacy and atrocities](#diplomacy-and-atrocities)
- [Base conquest](#base-conquest)
- [Probes](#probes)
- [Planetary Council](#planetary-council)
- [Effects plumbing](#effects-plumbing)
- [Outside the plan](#outside-the-plan)
- [Decisions](#decisions)

## Population

**Files:**
- `src/game/faction/base/population/PopulationManager.cpp`
- `src/game/faction/base/population/AssimilationTracker.cpp`
- `src/game/population/calculators/RiotCalculator.cpp`
- `src/game/population/calculators/GoldenAgeCalculator.cpp`
- `src/game/population/calculators/MoodLatch.cpp`
- `src/game/faction/base/BaseManager.cpp`
- `src/game/faction/RebelFactionPicker.cpp`

**Assessment:** Growth, pop loss, mood and assimilation are each decided inside the state owner that stores them. The forecasts re-derive the growth outcome rather than asking the same function. The calculators here already hold their formulas in config. The thresholds and branch structure around them are what sit in code.

### [D][X] POP-1 The growth outcome is decided in `PopulationManager` and re-derived by its forecasts
`PopulationManager.cpp:280` (`ApplyGrowth`), `:268` (`CommitPendingGrowth`), `:248` and `:261` (`WouldGrowThisTurn` / `WouldStarveThisTurn`).
- What it decides: growth empties the box; a full box at the population limit keeps `required / 2`; starvation empties the box; the deposit is floored at 0 and capped at the box.
- **Already in the plan:** `GrowthRules::ResolveGrowth`, now in commit 7.

### [D][X] POP-2 Which pop is lost is split between `PopulationManager` and `BaseManager`
`PopulationManager.cpp:89` (`SelectDoomedPop_`), `BaseManager.cpp:90` (constructor).
- `SelectDoomedPop_` ranks the pops and removes the lowest. The value it ranks by is a lambda passed from `BaseManager`'s constructor: a specialist is worth econ + labs + psych, a worker its tile's yield, an unassigned pop 0.
- Changing which citizen leaves means editing both files.
- **Added to the plan (commit 7):** `PopulationRules::SelectPopToRemove(const BaseManager&)` owns the ranking and the value. The valuator parameter in today's plan entry goes.

### [D] POP-3 The riot and golden-age conditions sit inside the latches
`RiotCalculator.cpp:13`, `GoldenAgeCalculator.cpp:13`.
- **Already in the plan:** `RiotConditionMet` / `GoldenAgeConditionMet`, now in commit 7.

### [D] POP-4 Forced riots: how they combine with the natural condition, and how they stack
`RiotCalculator.cpp:33` (`Commit`), `:55` (`ForceRiot`).
- A riot happens when the natural condition holds or a forced riot is pending.
- A second `ForceRiot` keeps the larger of the remaining and new turn counts. It does not add them.
- Both are mechanics: Incite Drone Riots twice on one base is a natural thing to ask about.
- **Added to the plan (commit 7):** `RiotConditionMet` takes the forced-riot count. New `PopulationRules::ForcedRiotTurns(int remaining, int added)`.

### [D] POP-5 Assimilation windows on capture are computed inside the tracker
`AssimilationTracker.cpp:64` (`NotifyCaptured`), `:22` (`MakeReversed_`).
- A fresh capture opens a window of `peak × decay` turns.
- Recapture by an earlier owner reverses that owner's window: peak becomes `elapsed / decay`, and the duration becomes the turns elapsed.
- An earlier owner's claim survives chained captures.
- **Added to the plan (commit 7):** `PopulationRules::AssimilationOnCapture(const AssimilationTracker&, FactionId_t previousOwner, FactionId_t newOwner, int peakDrones, int decayTurns)` returns the occupier window and the previous owner's new claim. The tracker stores them.

### [D] POP-6 Rebel faction weighting is computed in the mutation file
`RebelFactionPicker.cpp:88` (`PickRebelFactionAndTransfer`).
- What it decides: factions without bases are excluded; each candidate's weight is its `RebelJoinWeight` stat plus a distance bonus by configured mode.
- The weighted roll and the transfer belong in the mutation. The weights do not.
- **Added to the plan (commit 7):** `RebelRules::RebelCandidateWeights(const BaseManager&)`. The mutation rolls within the weights and transfers.

### [B] POP-7 Which workers become drones or talents follows container order
`PopulationManager.cpp:447` (`ApplyCompositionResult`).
- The calculator gives the counts. Which plain workers are seated is the order they sit in the container.
- It matters only if a drone on a worked tile should differ from a drone elsewhere.
- **Decision:** stays. No rule picks one worker over another better than container order.

### [B] POP-8 Withdrawn: the warning already lands before the player acts
`MoodLatch.cpp:26`, `config/turn_stages.json`.
- The turn runs `BaseGrowth` (the population change), then `Population` (forecast and warning), then `PlayerActions`, then `Mood` (commit). The warning reflects this turn's growth, and the riot is decided after the player has had a chance to act. The finding misread the latch.
- One narrow case remains: `Commit` does not require that a warning went out. A riot whose condition first appears during the player's own actions starts at `Mood` without one, for example after the player moves a police unit out.
- **Decision:** stays.

### [B] POP-9 A base is razed at size 0
`BaseManager.cpp:178` (the `OnPopLost` handler).
- It has a condition (size 0) but no choice. Today it is a rule of record.
- **Decision:** an engine invariant. It stays, and is listed under [Excluded](#excluded).

## Production and buildings

**Files:**
- `src/game/faction/base/production/ProductionManager.cpp`
- `src/game/faction/base/production/ProductionCompletion.cpp`
- `src/game/faction/base/BaseManager.cpp`
- `src/game/faction/base/buildings/BuildingManager.cpp`
- `src/game/faction/Military.cpp`
- `src/game/Faction.cpp`

**Assessment:** `BaseManager` is the production facade, and it makes most of the production decisions: cost, what can be built, when production halts, hurry and scrap eligibility, and the abandon-base forecast. "Riots halt production" is one flag, but what it stops is decided in three places. One config value, the retool threshold, also caps overflow.

### [D] PRD-1 The retooling forfeit
`ProductionManager.cpp:75` (`ApplyRetoolPenalty_`).
- **Already in the plan:** `ProductionRules::RetoolForfeit`, now in commit 8.

### [M][N] PRD-2 The retool threshold also caps production overflow
`ProductionManager.cpp:242` (`CompleteProduction_`) sets `m_mineralStockpile = std::min(leftover, retoolPenaltyThreshold)`.
- The same config value decides when retooling is free (`:88`) and how many minerals carry into the next item.
- A player changing one changes the other without knowing it.
- **Added to the plan (commit 8):** `ProductionRules::OverflowCarried(int leftover, const ProductionConfig_t&)`.
- **Outside the plan:** a separate config field for the cap, in the config package.

### [D] PRD-3 Item cost, the prototype surcharge and "is this a prototype" are decided in two state owners
`ProductionManager.cpp:113` (`GetMineralCost`), `Military.cpp:49` (`IsPrototype`).
- `ProductionManager` applies the surcharge and makes a stockpile cost 0.
- `Military` decides that a design is a prototype when any of its components has not been built yet.
- **Added to the plan (commit 8):** in `ProductionRules`, `int ItemMineralCost(const BaseManager&, const IConstructable&)` built on `ComputeProductionCost`, and `bool IsPrototype(const Faction&, const UnitDesign&)`.

### [X] PRD-4 What a halted base cannot do is decided in three places
`BaseManager.cpp:510` (`ApplyProduction`: no banking), `ProductionManager.cpp:155` with `ProductionCompletion.cpp:39` (no completion), `BaseManager.cpp:609` (`HurryProduction`: refused).
- The `DisableProduction` flag is one decision. Each site reads it separately.
- **Added to the plan (commit 8):** one rule per consequence, each reading the flag: `ProductionRules::CanBankProduction`, `CanCompleteProduction` and `CanHurry` (PRD-5). A switch for any one of them then has a single home.

### [D] PRD-5 Hurry is refused while production is halted
`BaseManager.cpp:601` (`HurryProduction`).
- Today the refusal is a throw inside the mutation, and the UI must know to check first.
- **Added to the plan (commit 8):** `ProductionRules::CanHurry(const BaseManager&)`, one of PRD-4's three rules. `HurryProduction` keeps the throw as a precondition check against it.

### [D] PRD-6 Secret projects cannot be scrapped
`BaseManager.cpp:642` (`QuoteScrapBuilding_`).
- **Added to the plan (commit 8):** `ProductionRules::CanScrapBuilding(const BuildingConfig_t&)`, beside `PlanScrapPayout`.

### [D] PRD-7 What a base can build
`BaseManager.cpp:470` (`GetConstructable`).
- What it lists: buildings from `BuildingManager`, stockpiles by tech, and every design. There is no domain or coastal check.
- The coastal question is the kind a player raises. Its answer belongs here.
- **Added to the plan (commit 8):** `ProductionRules::ConstructableItems(const BaseManager&)`.

### [D][X] PRD-8 The abandon-base forecast re-derives growth
`BaseManager.cpp:528` (`WouldCompletionAbandonBase`).
- It predicts next turn's size from `WouldGrowThisTurn` / `WouldStarveThisTurn`, floors starvation at 1, then predicts the completion's pop cost.
- **Added to the plan (commit 8):** `ProductionRules::CompletionAbandonsBase(const BaseManager&)`, built on `ResolveGrowth`.

### [X] PRD-9 Building eligibility is assembled twice
`BuildingManager.cpp:29` (`CanAddBuilding`), `:115` (`GetBuildingsAvailableForConstruction`).
- Both check uniqueness and secret-project availability. Only the list checks tech.
- That difference is intended: an effect may grant a building the faction cannot research yet.
- The shared half is still written twice, so a new requirement (coastal, say) has to go in both.
- **Added to the plan (commit 8):** `BuildingRules::MayHoldBuilding(const BuildingManager&, const BuildingConfig_t&)`. Both callers use it, and the list adds the tech gate.

### [D] PRD-10 The scrap refund goes to the nearest base, and only inside own territory
`Faction.cpp:652` (`ScrapDestinationFor_`).
- **Added to the plan (commit 8):** `ProductionRules::ScrapRefundBase(const Faction&, const Tile&)`.

### [D][N] PRD-11 A faction's first base without a headquarters gets "Headquarters"
`Faction.cpp:851` (`CreateBase`).
- The condition is a decision. The building id is hard-coded.
- **Added to the plan (commit 8):** `BaseRules::GrantsHeadquarters(const Faction&)`.
- **Outside the plan:** the building id as config.

### [B] PRD-12 A pre-empted build loses its minerals
`BaseManager.cpp:200`, with `ProductionManager.cpp:232`.
- "Pre-empted" means the base finishes an item it can no longer hold: a secret project another faction completed first, or a building the base already has.
- `CompleteProduction_` charges the full cost before the completion handler finds out, so the item is dropped and its minerals are gone. Only the usual overflow carries.
- I found nothing that moves a base off a project when a rival completes it, so the base keeps building toward it until it completes.
- The code's TODO says SMAC's rule is not recorded.
- **Decision:** the original game converts the lost item's minerals to energy. That is not implemented yet, and it is a place for a config option. It changes behaviour, so it is in the package.

### [B] PRD-13 A completion commits this turn's growth early
`BaseManager.cpp:214`, `:232`.
- `BaseProduction` runs before `BaseGrowth`, so a Hab Complex raises the cap before growth.
- A pop-cost item, a colony pod say, wants the post-growth size. Both cannot come first.
- So a completion commits the base's pending growth on the spot, inside `BaseProduction`, and `BaseGrowth` applies citizen intake later. Both completion branches repeat this.
- **Decision:** stays. Delaying on-complete effects to the growth stage would differ from the original game. If a config option for it comes later, that change extracts the rule.

### [B] PRD-14 Which copy is lost when a deployable building is destroyed
`Faction.cpp:282` (`NotifyBuildingDestroyed`), `OrbitalAttack.cpp:31` (`DestroyOneBuilding_`).
- Players see whether a copy has been deployed, not its turns remaining. Which cooling record goes is therefore bookkeeping.
- Whether the lost copy was deployed or ready is visible. Today it depends on which base `FindBaseWithBuilding` returns first:
  - a base with a cooling record loses a deployed copy, and the ready count is unchanged
  - a base without one loses a ready copy
- **Decision:** a deployed copy is lost first, across the whole faction. That changes behaviour when the first base found has no cooling record, so it is in the package.

## Economy

**Files:**
- `src/game/faction/base/resources/ResourceManager.cpp`
- `src/game/faction/base/resources/MineralSupport.cpp`
- `src/game/faction/EconomyManager.cpp`
- `src/game/stockpiles/StockpileConversion.cpp`
- `include/game/map/MapUtils.h`
- `src/game/map/WorkedTileIndex.cpp`

**Assessment:** The energy pipeline is tile energy plus commerce, then inefficiency, then the allocation split, then the econ/labs/psych modifiers. It lives in `ResourceManager` and `EconomyManager`. The getters and the turn agree, but each spells out the steps. Stockpile energy takes a third path that skips the modifiers. Unpaid upkeep throws by design: the code, the tests and `economy-system.md` record that there is no bankruptcy rule yet. Its home, when one comes, is `EconomyRules`.

### [D][N] ECO-1 Inefficiency
`ResourceManager.cpp:161` (`ApplyInefficiency_`).
- What it decides: the HQ is exempt; distance is tabletop-diagonal; a faction with no HQ counts as distance 16 (`Inefficiency.h:18`); the denominator comes from the Efficiency rating.
- **Added to the plan (commit 9):** `EconomyRules::EnergyAfterInefficiency(const BaseManager&, int energy)`.
- **Outside the plan:** the 16, in the config package.

### [X] ECO-2 The allocation pipeline is written out in three places
`ResourceManager.cpp:197`–`237` (getters), `:347` (`AllocateEnergy_`, the turn), `:306` (`AddAllocatedEnergy`, stockpiles), with the split rounding in `EconomyManager.cpp:57`–`69` (labs and psych floored, econ takes the remainder).
- The first two agree step for step.
- **Added to the plan (commit 9):** `EconomyRules::AllocateBaseEnergy(const BaseManager&, int energy)` returns an `EnergySplit_t`, and the split and its rounding move into `EconomyRules::SplitEnergy`. The getters and the turn call it. Stockpile energy keeps its own steps until ECO-7's change in the package.

### [D] ECO-3 What a base's worked yield is
`ResourceManager.cpp:116` (`ComputeWorked_`): worked tiles, plus supply crawlers, plus the base tile.
- **Added to the plan (commit 9):** `EconomyRules::WorkedYield(const BaseManager&)`.

### [D] ECO-4 Unit support comes out of production
`BaseManager.cpp:373` (`GetMineralsForProduction`).
- What it decides: support is deducted from production, floored at 0.
- **Added to the plan (commit 9):** `EconomyRules::MineralsForProduction(const BaseManager&)`.

### [D][M] ECO-5 Unit support: charges and the disband choice
`MineralSupport.cpp:31` (`ComputeCharges_`), `:84` (`ApplyMineralSupportAtBase`).
- `ComputeCharges_` decides two things: free slots go to units in home order, and native life on fungus is free.
- When the bank is short, the mutation picks the last charged unit to disband.
- **Added to the plan (commit 9):**
  - `EconomyRules::UnitSupportCost(const Unit&)` for the per-unit charge
  - `MineralSupportCost` keeps the slot order
  - `EconomyRules::UnitToDisbandForSupport(const BaseManager&)` for the choice

### [D][N] ECO-6 A base's work area is a geometry helper
`MapUtils.h:222` (`ForEachTileInWorkableArea`, `k_WorkableRadius = 2`, Euclidean).
- **Added to the plan (commit 9):** `BaseRules::ForEachWorkableTile(const Tile& baseTile, WorldMapT&, Fn&&)`.
- **Outside the plan:** the radius, in the config package.

### [B] ECO-7 Stockpile energy skips the econ/labs/psych modifiers
`StockpileConversion.cpp:101`, `ResourceManager.cpp:306`.
- Stockpile energy pays inefficiency and takes the split, but no econ, labs or psych bonus applies.
- **Decision:** stockpile energy is added to the base the same way collected energy is, and bonuses apply in one place. That changes behaviour, so it is in the package: `AddAllocatedEnergy` goes, and stockpile energy goes through `AllocateBaseEnergy`.

### [B] ECO-8 Worked-tile claims
`WorkedTileIndex.cpp:74` (`TryClaim`).
- A tile another base works cannot be claimed, and founding on a worked tile displaces the worker.
- **Decision:** a mechanic. Work areas may overlap, and a tile is worked by at most one base.
- **Added to the plan (commit 9):** `BaseRules::MayWorkTile(const BaseManager&, const Tile&)`, checked by `WorkerAssignmentManager::AssignWorker`. `TryClaim` keeps exclusive claims as its invariant.
- The displacement on founding is unconditional and stays.

## Research and social engineering

**Files:**
- `src/game/faction/ResearchManager.cpp`
- `src/game/stages/ResearchAccumulation.cpp`
- `src/game/faction/SocialEngineeringManager.cpp`

**Assessment:** The state owners decide availability and carry-over. The tech cost formula is Lua, but which inputs feed it is chosen in code, the same pattern as eco damage and commerce.

### [D] RES-1 Which inputs feed tech cost
`ResearchManager.cpp:111` (`ComputePointsNeeded_`).
- **Added to the plan (commit 9):** `ResearchRules::TechCostInputs(const Faction&, const TechConfig_t&)`. The formula stays in its calculator.

### [D] RES-2 Surplus research carries over
`ResearchManager.cpp:174` (`DiscoverTech`).
- **Added to the plan (commit 9):** `ResearchRules::ResearchCarriedOver(int accumulated, int cost)`.

### [D] RES-3 A tech is available when every prerequisite is known
`ResearchManager.cpp:216` (`GetAvailableTechs`).
- **Added to the plan (commit 9):** `ResearchRules::AvailableTechs(const Faction&)`. `GetAvailableTechs` delegates to it.

### [D] RES-4 A policy is available when its tech is known
`SocialEngineeringManager.cpp:83` (`GetAvailablePolicies`).
- `SetActivePolicy` has no legality or switching-cost check. When that lands, it goes beside this.
- **Added to the plan (commit 9):** `SocialEngineeringRules::AvailablePolicies(const Faction&)`.

**Stays with its mutation:** the research stage discovers techs in a loop while points suffice (`ResearchAccumulation.cpp:18`). A one-tech-per-turn switch would extract that loop's condition.

## Ecology

**Files:**
- `src/game/ecology/BaseEcology.cpp`
- `src/game/atrocities/AtrocityLedger.cpp`
- `src/game/stages/EcoDamage.cpp`

**Assessment:** Eco damage is a Lua formula fed by inputs chosen in a cache. The fungal pop, the mechanic players see, is decided entirely inside a turn stage.

### [D] ECL-1 Which inputs feed eco damage
`BaseEcology.cpp:71` (`CollectInputs_`).
- **Added to the plan (commit 9):** `EcologyRules::EcoDamageInputs(const BaseManager&)`.

### [D] ECL-2 Atrocities add virtual eco minerals, computed inside the ledger
`AtrocityLedger.cpp:57` (`EcoVirtualMinerals`).
- **Added to the plan (commit 9):** `EcologyRules::AtrocityEcoMinerals(const AtrocityLedger&, FactionId_t)`. The ledger keeps its records.

### [D] ECL-3 Fungal pops are decided in the turn stage
`EcoDamage.cpp:30` (`ExecuteImpl`), `:64` (`PickPopTile_`).
- What it decides: the chance is `min(maxChancePercent, eco damage)` per base per turn; the tile is a random workable tile that is neither base nor fungus.
- **Added to the plan (commit 9):** `EcologyRules::FungalPopChance(const BaseManager&)` and `FungalPopTiles(const BaseManager&)`. The stage rolls within them.

## Combat

**Files:**
- `src/game/units/CombatResolver.cpp`
- `src/game/units/CombatStrength.cpp`
- `src/game/units/StackCollateral.cpp`
- `src/game/units/PlanetPearls.cpp`
- `src/game/units/AttackRules.cpp`
- `src/game/units/InterceptRules.cpp`
- `src/game/units/UnitOrderExecutor.cpp`
- `src/game/orbital/OrbitalAttack.cpp`
- `src/game/map/Explosion.cpp`

**Assessment:** `CombatResolver` and `ApplyStackCollateral` were already in the plan. The rest of combat's decisions sit in `UnitOrderExecutor`: attack cost, promotion, bombard consequences and the bombard hostile act. The interceptor and the satellite attack decide their outcome order in the mutation. `CollectBombardTargets` is the one Rules function in the codebase that decides two mechanics.

### [D] CMB-1 Which defender stat a duel uses
- **Already in the plan:** `ResolveCombatStrength` takes the engagement. Now in commit 10.

### [D][N] CMB-2 Round winner and damage
`CombatResolver.cpp:152`–`174`.
- Equal rolls favour the defender. Damage is `k_roundDamage = 1` (`CombatResolver.h:105`).
- **Already in the plan:** `CombatRules::ResolveCombatRound`. Now in commit 10.
- **Outside the plan:** the round damage, in the config package.

### [D][N] CMB-3 The half-HP disengage gate
`CombatResolver.cpp:56`.
- **Already in the plan:** `CombatRules::PastDisengageThreshold`. Now in commit 10.

### [D] CMB-4 Stack collateral
`StackCollateral.cpp:78`.
- **Already in the plan:** `CombatRules::PlanStackCollateral`. Now in commit 10.

### [M] CMB-5 `CollectBombardTargets` decides both whether a duel happens and what a strike hits
`AttackRules.cpp:170`.
- If any hostile on the tile has `Bombard`, the attack is a duel against the first legal one, and the strike list is cleared. If none is legal, nothing is hit. Otherwise every legal hostile is struck.
- A switch for artillery duels would sit in a function that also decides the strike targets.
- **Added to the plan (commit 10):** `AttackRules::ArtilleryDuelTarget(const Unit&, const Tile&)` and `AttackRules::BombardStrikeTargets(const Unit&, const Tile&)`. `BombardTargeting_t` goes.

### [D] CMB-6 The defender is the first visible hostile in tile order
`AttackRules.cpp:105` (`FindVisibleHostileOnTile`), called by `FindAttackableHostileOnTile` (`:121`) and `UnitOrderExecutor::TryAttack` (`UnitOrderExecutor.cpp:366`).
- No rule picks the best defender. The duel opponent in CMB-5 is chosen the same way.
- **Added to the plan (commit 10):** `AttackRules::SelectDefender(const Unit& attacker, const Tile&)`, with today's order. `FindAttackableHostileOnTile` keeps the legality checks and calls it.
- **Decision:** the defender is the one with the greatest chance of success. Combat odds are a stub for now, so the plan builds the ranking around a stub that keeps today's first-unit choice (commit 10).

### [D] CMB-7 Who rolls for promotion after combat
`UnitOrderExecutor.cpp:72` (`PromoteOnKill_`).
- The survivor of a kill rolls. Nobody rolls when both units or neither die.
- **Added to the plan (commit 10):** `MoraleRules::PromotionCandidate(const CombatResult_t&)`. The executor calls `MoraleMutations::TryPromote` on the answer.

### [D] CMB-8 What an attack costs
`UnitOrderExecutor.cpp:85` (`SpendAttackAction_`), with the callers in `TryAttack` and `TryBombard`.
- An attack costs one move point unless the unit has `AttackingEndsTurn`. A bombard always ends the turn.
- **Added to the plan (commit 10):** `AttackRules::AttackMoveFragments(const Unit&, bool bBombard)`.

### [D] CMB-9 Bombarding an empty tile destroys one improvement
`UnitOrderExecutor.cpp:473` (`TryBombard`).
- On an unoccupied tile, a bombard destroys one random non-base improvement.
- **Added to the plan (commit 10):** `AttackRules::BombardImprovementTargets(const Unit&, const Tile&)`. It is empty when the tile is occupied and replaces `NonBaseImprovementIds`. The executor picks one.

### [D] CMB-10 The interceptor's outcome order is decided in the mutation
`InterceptRules.cpp:275` (`TryInterceptAttack`), with `:99`, `:167` and `:215`.
- Candidates are tried in a fixed order: faction-global, base, unit, world grantors, council, tile. The first ready hit wins.
- A hit destroys the attacker outright. A cooldown is deployed even on a miss. A failed roll may destroy the source.
- **Added to the plan (commit 10):**
  - new `combat/InterceptRules.h`: `CollectInterceptSources(const Unit& attacker, const Unit& defender)`, ordered, and `InterceptOutcome_t ResolveIntercept(const InterceptSource_t&, int hitRoll, int destroyRoll)`
  - commit 4's rename to `InterceptMutations` stands, and the mutation applies the outcome

### [D] CMB-11 The satellite attack's outcome order is decided in the mutation
`OrbitalAttack.cpp:80` (`TryAttackSatellite`).
- The cooldown is deployed on a miss. The attacker is destroyed only on a miss and a second failed roll.
- **Added to the plan (commit 10):** `BuildingRules::ResolveOrbitalStrike(const OrbitalAttackEffect_t&, int hitRoll, int destroyRoll)`.
- **Decision:** CMB-10 and CMB-11 have the same shape but are two mechanics: satellite-to-unit and satellite-to-satellite. They keep two rules.

### [D] CMB-12 Blast shape and consequences are decided in the mutation
`Explosion.cpp:88` (`ApplyExplosion`), with `:32`, `:57` and `:69`.
- The blast is a Chebyshev disk. Every unit but the spared one is destroyed, bases are razed, improvements are removed, and tiles drop one level.
- **Added to the plan (commit 10):** new `combat/ExplosionRules.h`: `ExplosionPlan_t PlanExplosion(const Tile& centre, int radius, const Unit* pSpared)`, giving the tiles and what happens on each. `ApplyExplosion` applies it.

### [X] CMB-13 "Wild native" is defined twice
`StackCollateral.cpp:70` and `PlanetPearls.cpp:20`, identical.
- **Added to the plan (commit 10):** `natives/NativeRules.h`: `IsWildNative(const Unit&)`.

**Stays with its mutation:** a defender that survives a bombard loses its order (`UnitOrderExecutor.cpp:115`).

## Movement

**Files:**
- `src/game/units/UnitOrderExecutor.cpp`
- `src/game/units/FoundBaseRules.cpp`
- `src/game/units/FuelRules.cpp`
- `src/game/units/EvacuateTerritoryEffects.cpp`
- `src/game/units/MovementRules.cpp`
- `src/game/effects/TriggeredEffectDispatch.cpp`
- `src/game/map/FungalBloom.cpp`
- `src/game/map/TerritoryMap.cpp`
- `src/game/faction/FactionVisibleMap.cpp`

**Assessment:** Two central movement mechanics are decided in the executor: how a unit pays for a tile it cannot afford, and which units may found a base on water. "Can a unit of this domain sit on this tile" is answered three times. Territory claims are computed inside the `TerritoryMap` index.

### [D] MOV-1 How a unit pays for a tile it cannot afford
`UnitOrderExecutor.cpp:293` (`SpendMovesAndEnter_`).
- Any tile admits a unit with any moves left: the cost clamps to what remains. The exception is fungus without a road, river or matching clamp, whose full cost is banked across turns (`chargeFragmentsPaid`) before the unit enters. Fungus entry ends the turn.
- `MoveCostCalculator::EntryTerms` decides when the full cost is required. The executor decides how it is paid.
- **Added to the plan (commit 11):** `MovementRules::ResolveEntryPayment(const EntryTerms_t&, int available, int paidSoFar)` returns whether the unit enters, what it spends, and what it has paid afterwards.
- **Decision:** this matches the original, as far as you know. It is a candidate for a config option, whose switches would sit in `MoveCostRules::EntryTerms` (when the full cost is required) and `ResolveEntryPayment` (how it is paid).

### [D] MOV-2 Airdrop landing
`UnitOrderExecutor.cpp:218` (`TryAirdrop`).
- `AirdropLandingDamageHp` is already a rule.
- Two decisions sit in the executor instead: landing damage never kills (HP floor 1), and a non-combat unit ends its turn after a drop while a combat unit keeps its moves.
- **Added to the plan (commit 11):** `AirdropRules::AirdropLanding(const Unit&, const Tile&)` returns the HP afterwards and whether the turn ends.

### [D][X] MOV-3 Founding on water is decided outside `CanFoundBaseAt`
`UnitOrderExecutor.cpp:549` (`TryFoundBase`).
- The executor refuses any unit but a sea unit on water, and lets a sea unit's base occupy water. `CanFoundBaseAt` never sees either decision.
- The UI cannot ask, because no rule answers.
- **Added to the plan (commit 11):** `FoundBaseRules::CanFoundBase(const Unit&)` (the `FoundBase` flag, the domain check and `CanFoundBaseAt`) and `FoundBaseRules::FoundedBaseMayOccupyWater(const Unit&)`.

### [D][X] MOV-4 The end-of-turn fuel outcome is decided twice
`FuelRules.cpp:185` (`ProcessFuelAtTurnEnd`), `:105` (`WouldBeDestroyedWithoutRefuelThisTurn_`).
- **Already in the plan:** `FuelRules::ResolveEndOfTurnFuel`. Now in commit 11.

### [D] MOV-5 Who is evacuated
`EvacuateTerritoryEffects.cpp:26`, `:42`.
- A guest unit is evacuated from host territory only when its owner is known to the host.
- `FindNearestOwnTerritoryTile` is already a rule, and the group-then-individual placement stays in the mutation because it interleaves with placement state.
- **Added to the plan (commit 11):** `EvacuateTerritoryRules::UnitsToEvacuate(const Faction& guest, FactionId_t host)` and `UnitsSharingTilesWith(const Faction& guest, const Faction& host)`.

### [X] MOV-6 "Can a unit of this domain sit on this tile" is answered three times
- `TriggeredEffectDispatch.cpp:232` (`DesignCanHoldTile_`): harbour, or surface by domain.
- `FungalBloom.cpp:31` (`DomainCanHold_`): surface by domain.
- `MovementRules::CanHoldTileWithoutCarrier`: interaction grid, or harbour.
- The two surface checks are identical. The grid check can differ from them.
- **Decision:** placement uses the interaction grid. That changes behaviour wherever the grid and the surface check disagree, so it is in the package, and the plan leaves both surface copies alone.

### [D][N] MOV-7 Where granted units appear
`TriggeredEffectDispatch.cpp:205` (`ResolveAnchorBase_`), `:256` (`PlaceableTilesNear_`).
- Granted units home at an anchor base. The anchor is the context base if it is the faction's own, else the nearest own base to the tile, else the HQ, else the first base.
- They are placed within radius 2, origin first.
- **Added to the plan (commit 11):** new `units/UnitPlacementRules.h`: `GrantAnchorBase(const Faction&, const BaseManager* pContextBase, const Tile* pTile)` and `PlacementTiles(const Tile& origin, const UnitDesign&, FactionId_t, int needed)`.
- **Outside the plan:** the radius, in the config package.

### [D][N] MOV-8 Territory claims are computed inside the index
`TerritoryMap.cpp:45` (`ClaimFromBase_`), `:122` (`Rebuild`).
- Land bases claim radius 7 and sea bases radius 3 (`:20`–`21`), by same-surface flood fill. The nearest base wins, and ties go to the lower base id.
- **Added to the plan (commit 11):** new `map/TerritoryRules.h`: `ClaimTerritory(const WorldMap&, const std::vector<const BaseManager*>&)` returns the owner grid. `TerritoryMap::Rebuild` stores it.
- **Outside the plan:** the radii, in the config package.

### [N] MOV-9 Base sight radius
`FactionVisibleMap.cpp:27`: `k_BaseVisionRadius = 2`.
- **Outside the plan:** in the config package.

## Terraform and map

**Files:**
- `src/game/map/ElevationChange.cpp`
- `src/game/map/TerraformSpread.cpp`
- `src/game/map/SurfaceOccupancy.cpp`
- `src/game/map/FungalBloom.cpp`
- `src/game/units/UnitOrderExecutor.cpp`

**Assessment:** Each `map/` mutation chooses its own targets. The plan already moves the spread score. The rest of the choice, and the elevation cascade, surface flips and bloom tiles, follow the same pattern.

### [D] TFM-1 Raising or lowering a tile drags its neighbours
`ElevationChange.cpp:128` (`RelaxAdjacentSlopes_`).
- Neighbours are pulled to within `maxAdjacentDifferenceMeters`, cascading outward. The cascade is computed while writing.
- **Added to the plan (commit 11):** `TerraformRules::PlanElevationChange(const Tile& origin, int deltaMeters, int floorMeters, int ceilingMeters)` returns the new elevation per tile, computed on a working copy. `ApplyElevationDelta` and `LowerTilesOneLevel` apply it, then reconcile surfaces and rivers as today.

### [D][N] TFM-2 Where forest and kelp spread
`TerraformSpread.cpp:79` (`PickBestSpreadNeighbor_`), with its eligibility helpers.
- Eligibility: the altitude gate; rocky land excluded; bases and existing copies excluded. The score is `4 × (3 − moisture) − rockiness`.
- The plan moved only the score.
- **Added to the plan (commit 11):** `TerraformRules::PickSpreadTarget(const Tile& origin)` takes the whole choice.
- **Outside the plan:** the score weights, in the config package.

### [N] TFM-3 Spread attempts per turn
`TerraformSpread.cpp:110`: `tileCount / (turn / 4 + 32)`.
- Already moving to `TerraformRules`.
- **Outside the plan:** the constants, in the config package.

### [D] TFM-4 What a land/sea flip does
`SurfaceOccupancy.cpp:102` (`ReconcileSurfaceFlips`).
- Improvements of the wrong domain are removed. A submerged base is razed unless it may occupy water.
- **Added to the plan (commit 11):** `TerraformRules::SurfaceFlipConsequences(const Tile&, bool bNowWater)` returns the improvements to remove and whether the base is razed.

### [D] TFM-5 Terraform progress is one turn per former per turn
`UnitOrderExecutor.cpp:776` (`Execute_(TerraformOrder_t)`).
- Formers on one tile do not combine, and nothing changes the rate.
- **Decision:** formers building the same project on the same tile share its countdown, and a former building something else works alone. That is in the package.

### [D] TFM-6 Which tiles bloom and which lifeforms appear
`FungalBloom.cpp:124` (`SelectBloomTiles_`), `:58` (`EligibleNatives_`), `:74` (`PlaceLifeforms_`).
- Tiles: the origin unless it is a base, plus shuffled radius-1 neighbours that are neither base nor fungus.
- Lifeforms appear only on new, habitable tiles, with a random eligible design.
- **Added to the plan (commit 11):** `ecology/EcologyRules.h`: `BloomCandidateTiles(const Tile& origin)` and `EligibleLifeforms(const Tile&)`. The mutation shuffles and rolls within them.

### [B] TFM-7 Earthquake magnitude is capped at the full elevation span
`ElevationChange.cpp:235` (`ApplyEarthquake`).
- The rolls stay within a limit. Only the cap is a decision.
- **Decision:** make it a rule.
- **Added to the plan (commit 11):** `TerraformRules::EarthquakeDeltaMeters(std::span<const int> levelRollsMeters, const ElevationRulesConfig_t&)`.

**Stays with its mutation:** starting a terraform pays its energy up front and ends the former's turn (`UnitOrderExecutor.cpp:602`).

## Diplomacy and atrocities

**Files:**
- `src/game/faction/DiplomaticTransitionEffects.cpp`
- `src/game/atrocities/AtrocityEffects.cpp`
- `src/game/units/UnitOrderExecutor.cpp`

**Assessment:** The rule pieces exist (`AtrocityRules`, `DiplomaticPermissionRules`), but the mutations assemble the outcome from them. The defensive-obligation mode is the one existing config switch whose dispatch sits in a mutation.

### [D] DIP-1 Whether an attributed hostile act causes Vendetta
`DiplomaticTransitionEffects.cpp:183` (`ApplyHostileAct`).
- **Already in the plan:** `VendettaFromHostileAct`. Now in commit 12.

### [D] DIP-2 Only a declaration withdraws units from each other's territory
`DiplomaticTransitionEffects.cpp:50` (`EnterVendetta_`).
- A sneak attack does not withdraw them.
- **Added to the plan (commit 12):** `DiplomaticTransitionRules::VendettaWithdrawsTerritory(VendettaKind_t)`.

### [D] DIP-3 The defensive-obligation mode is dispatched in the mutation
`DiplomaticTransitionEffects.cpp:195` (`HonorDefensiveObligation`).
- `JoinAsDefender` enters Vendetta quietly. `SeparateDeclaration` declares, which raises the partner's own obligations.
- **Added to the plan (commit 12):** `DiplomaticTransitionRules::HonoredObligationCascades()`. The mutation declares or enters on its answer.

### [D] DIP-4 Who a bombard offends
`UnitOrderExecutor.cpp:430` (`BombardHostileActs_`).
- Each struck unit's owner is offended once, with attribution OR-ed across hits.
- So is the territory owner, when an unoccupied improved tile is hit.
- **Added to the plan (commit 12):** `DiplomaticTransitionRules::BombardHostileActs(const Unit& attacker, const Tile& target)`, built on CMB-5's two target functions.

### [D] DIP-5 Who joins a universal Vendetta
`AtrocityEffects.cpp:58` (`ApplyUniversalVendetta_`).
- Joiners are AI factions, excluding the perpetrator, the victim and anyone already at Vendetta.
- **Added to the plan (commit 12):** `AtrocityRules::UniversalVendettaJoiners(FactionId_t perpetrator, std::optional<FactionId_t> victim)`.

### [D] DIP-6 The atrocity outcome is assembled in the mutation
`AtrocityEffects.cpp:148` (`CommitAtrocity`).
- What it assembles: the excuse (Charter repealed, Progenitor party); whether it counts; the effective severity against the player or AI threshold; sanction years; and which penalties follow.
- **Added to the plan (commit 12):** `AtrocityOutcome_t AtrocityRules::ResolveAtrocity(const Faction& perpetrator, const Faction* pVictim, AtrocitySeverityId_t authored)`. `CommitAtrocity` records, applies the hostile act, extends the sanction and applies the penalties it names.

## Base conquest

**Files:**
- `src/game/units/BaseConquestEffects.cpp`
- `src/game/units/BaseConquestRules.cpp`

**Assessment:** The plan already moves the facility range and the species-clash split. The rest of the flow follows the same pattern and joins them in one rule set: what a capture does, what a native raid does, and which of them happens.

### [D] BCQ-1 How many facilities a capture destroys
`BaseConquestEffects.cpp:71`.
- **Already in the plan.** Now in commit 12, inside BCQ-4's `PlanCapture`.

### [D][N] BCQ-2 Species clash: pops removed and pods that flee
`BaseConquestEffects.cpp:160`: size − 1 removed, half of those flee.
- **Already in the plan.** Now in commit 12, inside `PlanCapture`.
- **Outside the plan:** the half, in the config package.

### [D][N] BCQ-3 A native raid
`BaseConquestEffects.cpp:202` (`ApplyNativeRaid_`).
- A 50/50 coin decides between a facility and a pop. A facility is forced when the base has no pop. The raider dies.
- **Added to the plan (commit 12):** `BaseConquestRules::PlanNativeRaid(const BaseManager&)` returns the facility candidates, the facility chance (0, 50 or 100) and whether a pop can be lost.
- **Outside the plan:** the 50, in the config package.

### [D] BCQ-4 Which conquest happens, and what a capture does
`BaseConquestEffects.cpp:232` (`ApplyCapture_`), `:273`, `:306`.
- Which conquest happens:
  - On entry: an own or garrisoned base means nothing; a native raids; a unit that may capture, captures.
  - On a last-defender kill: the casualty pop loss, then a raid if the attacker is native.
- What a capture does: cross-species captures skip the capture pop loss and clash instead; the capturer is fully repaired unless `NoConquestRepair`.
- **Added to the plan (commit 12):**
  - `BaseConquestRules::ConquestOnEntry(const Unit&, const BaseManager&)` and `ConquestOnLastDefenderKill(const Unit&, const BaseManager&)` return the conquest kind
  - `CaptureOutcome_t PlanCapture(const Unit& capturer, const BaseManager&)` returns the facility range, the pop loss or clash split, and whether the capturer is repaired

## Probes

**Files:**
- `src/game/units/ProbeActionExecutor.cpp`
- `src/game/units/ProbeActionEffects.cpp`
- `src/game/units/ProbeRules.cpp`

**Assessment:** `ProbeRules` holds the formulas. The executor still picks the chance scales and the risk, and the mutations pick their candidates.

### [D][N] PRB-1 Mission risk
`ProbeActionExecutor.cpp:169` (`ResolveMissionRisk_`).
- What it decides: repeat risk, and a risk floor of 2 for sabotaging a headquarters facility.
- **Already in the plan:** `ProbeRules::MissionRisk`. Now in commit 12.
- **Outside the plan:** the 2, in the config package.

### [D][N] PRB-2 Energy theft amount
`ProbeActionEffects.cpp:115`.
- **Already in the plan:** `ProbeRules::StealEnergyAmount`. Now in commit 12.
- **Outside the plan:** the formula's constants, in the config package.

### [D] PRB-3 Which techs can be stolen
`ProbeActionEffects.cpp:82` (`PickStealableTech_`).
- Candidates are techs the actor could research that the target knows.
- **Added to the plan (commit 12):** `ProbeRules::StealableTechs(const Faction& actor, const Faction& target)`. The mutation picks one.

### [D] PRB-4 Which scales apply to the chances
`ProbeActionExecutor.cpp:190` (`FillProbeChances_`).
- The failure scale (Algorithmic Enhancement) applies only when the target does not block probe teams. The success scale (Hunter-Seeker) always applies.
- **Added to the plan (commit 12):** `ProbeRules::ProbeChancesFor(const Unit&, const ProbeTarget_t&, int risk)`. It replaces `FillProbeChances_` and calls `ComputeProbeChances`.

### [X] PRB-5 The automatic-success threshold differs between the chance and the roll
`ProbeRules.cpp:201` (`ComputeProbeChances`: `risk < 0`), `:238` (`RollProbeAction`: `risk <= 0`).
- At risk 0 the chance is `100 × successScale`, but the roll always succeeds.
- **Decision:** risk 0 rolls normally; in Thinker such a mission can fail. That is a bug fix in the package.

### [B] PRB-6 A random sabotage that destroys nothing wipes production
`ProbeActionEffects.cpp:145` (`ApplySabotage_`).
- The fallback depends on what the configured effects did. A rule cannot predict it without running them.
- **Decision:** stays.

**Stays with its mutation:**
- the probe outcome consequences (`ProbeActionExecutor.cpp:213`): a failed or captured probe is destroyed, and a successful one rolls for promotion and ends its turn
- the hostile act applies before the success check
- Assassinate zeroes research (`ProbeActionEffects.cpp:196`)

## Planetary Council

**Files:**
- `src/game/council/PlanetaryCouncil.cpp`
- `src/game/GameState.cpp`

**Assessment:** The plan's council commit moves the vote lifecycle into `CouncilVoteExecutor`. Before this review it left vote weight, candidates and `CanPropose` on `PlanetaryCouncil` and moved the tallies into the executor. All of these are rules.

### [D] CNC-1 Vote weight
`PlanetaryCouncil.cpp:195` (`ComputeVoteWeight`).
- Votes are population (or 1, by mode), passed through the `CouncilVotes` stat.
- **Added to the plan (commit 13):** `CouncilRules::VoteWeight(const PlanetaryCouncil&, const Faction&, CouncilVoteWeight_t)`.

### [D][N] CNC-2 Governor candidates
`PlanetaryCouncil.cpp:217` (`GovernorCandidates`).
- The two most populous members stand, with ties going to the lower faction id.
- **Added to the plan (commit 13):** `CouncilRules::GovernorCandidates(const PlanetaryCouncil&)`.
- **Decision:** rank candidates by votes. That is in the package, and the count of 2 goes to the config package.

### [D] CNC-3 Tallies
`PlanetaryCouncil.cpp:588` (`TallyStandard_`), `:625` (`TallyElection_`).
- A proposal passes on a threshold share of total weight, counting abstainers and non-voters, or else on yea > nay.
- An election goes to the most votes, with ties to the lowest id; the threshold applies.
- **Added to the plan (commit 13):** `CouncilRules::ProposalPasses(const PlanetaryCouncil&, const PendingProposal_t&)` and `CouncilRules::ElectionWinner(const PlanetaryCouncil&, const PendingProposal_t&)`.

### [D] CNC-4 The veto
`PlanetaryCouncil.cpp:487` (`VetoPending`), `:505` (`VetoUnanimouslyOverruled_`).
- Elections cannot be vetoed. A veto is overruled only when every non-governor member votes yea.
- **Added to the plan (commit 13):** `CouncilRules::CanVeto(const PlanetaryCouncil&, const Faction&, const PendingProposal_t&)` and `CouncilRules::VetoOverruled(const PlanetaryCouncil&, const PendingProposal_t&)`.

### [D] CNC-5 What may be proposed
`PlanetaryCouncil.cpp:265` (`CanPropose`).
- The checks: membership, tech, prerequisites, repeal targets, rule flags and the repeat restriction.
- **Added to the plan (commit 13):** `CouncilRules::CanPropose(const PlanetaryCouncil&, const Faction&, const std::string& proposalId)`.

**Stays with its mutation:**
- calling a vote makes every member known to every other
- an expelled governor loses office
- membership is fixed when the council is created (`GameState.cpp:754`)

## Effects plumbing

### [X][B] EFF-1 The list of world-effect sources is written twice
`GameState.cpp:297` (`CollectSessionWorldEffects`), `:319` (`CollectWorldExtras`).
- This is plumbing, not a mechanic. A "world events do not apply" switch would have to touch both.
- **Decision:** merge them.
- **Added to the plan (commit 2):** one private `AppendWorldSources_` that both call.

## Outside the plan

These change behaviour or config, so they do not belong in a plan whose rule is "game behaviour does not change". Together they are the separate package.

### Behaviour

- **Terraform pooling (TFM-5), decided.**
  - Formers on the same tile building the same project share one countdown, and each former working it takes a turn off it.
  - A former on the same tile building something else works on its own project.
  - Today each order counts down alone (`UnitOrderExecutor.cpp:776`).
  - A shared countdown is new save data: progress kept per tile and project rather than per order. The rule goes in `TerraformRules`.
- **Governor candidates (CNC-2), decided.** Candidates are ranked by votes (`CouncilRules::VoteWeight`), not raw population. The count of 2 moves to config.
- **Probe success at risk 0 (PRB-5), decided, a bug.**
  - Many configured missions are risk 0. The Hunter-Seeker Algorithm blocks probe teams, which only Algorithmic Enhancement ignores (`ProbeRules.cpp:292`), and applies `probe_success_scale` −50%.
  - In Thinker, a risk-0 mission against the Algorithm's owner can fail, so risk 0 rolls normally.
  - `RollProbeAction` stops treating risk 0 as automatic and rolls against the success rate `ComputeProbeChances` gives. Only negative risk is automatic, as the displayed chance already says, so the decision lives in one place.
- **Probe repeat record (PRB-1), a bug.** `ProbeActionExecutor::m_probedBases` (`ProbeActionExecutor.cpp:131`) is never cleared and is not saved. Repeat risk is "ever" within a session and resets on load.
- **Stockpile energy (ECO-7), decided.**
  - Stockpile energy is added to the base the same way collected energy is: through `AllocateBaseEnergy`, so the econ/labs/psych bonuses apply to it in the one function that applies them. `AddAllocatedEnergy` goes.
  - "One place" means one place in the code. Stockpiles convert in `BaseProduction`, after `ResourceCollection` has allocated the base's energy, so the function runs on two amounts each turn. With percentage bonuses and flooring, that can come out a point different from allocating the sum once. Whether the original game allocates once is still to be found by experiment.
- **Pre-empted builds (PRD-12), decided.**
  - A base that finishes an item it can no longer hold converts the item's minerals to energy, as the original game does.
  - The decision goes in `ProductionRules`, which reads its switch from `ProductionConfig_t`. The completion handler in `BaseManager` applies the outcome.
- **Lost deployable copies (PRD-14), decided.**
  - When a faction loses one copy of a deployable building, it loses a deployed copy first, across all its bases.
  - New `BuildingRules::BaseToLoseCopy(const Faction&, const BuildingId_t&)`: a base with a cooling record for that building, else the first base holding it. `DestroyOneBuilding_` uses it.
  - `NotifyBuildingDestroyed` keeps dropping that base's latest record.
- **Placement uses the interaction grid (MOV-6), decided.**
  - Granted units and bloom lifeforms are placed where the Enter grid allows, as moving units are.
  - The grid query takes a unit today, and placement has only a design. The package adds a design form of the check and replaces `DesignCanHoldTile_` and `DomainCanHold_` with it.
  - Placement can change wherever the grid and the plain surface check disagree.

### Hard-coded numbers to config

These go to the game-config expansion, not the behaviour package: moving a number into config is parser and config-file work, like the rest of that phase.

| Number | Where | Finding |
|--------|-------|---------|
| Overflow cap (shares the retool threshold) | `ProductionManager.cpp:242` | PRD-2 |
| Auto-HQ building id `"Headquarters"` | `Faction.cpp:851` | PRD-11 |
| No-HQ inefficiency distance 16 | `Inefficiency.h:18` | ECO-1 |
| Work radius 2 | `MapUtils.h:224` | ECO-6 |
| Round damage 1 | `CombatResolver.h:105` | CMB-2 |
| Disengage gate at half HP | `CombatResolver.cpp:56` | CMB-3 |
| Granted-unit placement radius 2 | `TriggeredEffectDispatch.cpp:256` | MOV-7 |
| Territory radius 7 land, 3 sea | `TerritoryMap.cpp:20`–`21` | MOV-8 |
| Base sight radius 2 | `FactionVisibleMap.cpp:27` | MOV-9 |
| Base founding separation 3 | `FoundBaseRules.h:18` | — |
| Spread score `4 × (3 − moisture) − rockiness` | `TerraformSpread.cpp` | TFM-2 |
| Spread attempts `tiles / (turn / 4 + 32)` | `TerraformSpread.cpp:110` | TFM-3 |
| Species clash: half the removed pops flee | `BaseConquestEffects.cpp:160` | BCQ-2 |
| Native raid 50/50 | `BaseConquestEffects.cpp:202` | BCQ-3 |
| Headquarters sabotage risk floor 2 | `ProbeActionExecutor.cpp:169` | PRB-1 |
| Energy theft constants | `ProbeActionEffects.cpp:115` | PRB-2 |
| No-HQ probe distance 12 | `ProbeRules.cpp:30` | — |
| Governor candidate count 2 | `PlanetaryCouncil.cpp:217` | CNC-2 |

The two no-HQ distances (16 for inefficiency, 12 for probes) are separate SMAC numbers, not a duplicate.

### Documentation

`docs/game-rules/turn-structure.md` lists Base growth before Unit support and Base production, and leaves out `PostActionsProduction` and `Mood`. `config/turn_stages.json` and `docs/architecture/turn-system.md` have the real order.

### Organisation

`Faction::TransferBaseTo` and `Faction::TransferUnitTo` (`Faction.cpp:548`, `:614`) write another faction. That is the same drift the plan fixes in `PlanetaryCouncil`: a state owner changing state outside itself. Moving them to `FactionMutations` touches every capture, trade and rebellion path, so it is listed here rather than added to an already long plan. What a transfer resets is unconditional and stays with whichever mutation does it.

## Decisions

| Finding | Decision | Where it goes |
|---------|----------|---------------|
| POP-7 | Container order is as good as any rule | Stays |
| POP-8 | Withdrawn: the warning already comes before the player acts | Stays |
| POP-9 | An engine invariant | Stays |
| PRD-12 | Convert the minerals to energy, as the original does, behind a config option | Package |
| PRD-13 | Keep today's order; delaying differs from the original | Stays |
| PRD-14 | A deployed copy is lost first, faction-wide | Package |
| ECO-7 | One energy path; bonuses applied in one place in the code. Kept simple for now; a proper fix comes later | Package |
| ECO-8 | A mechanic: overlapping areas, one worker per tile | Plan, commit 9 |
| CMB-10/11 | Two mechanics, two rules | Plan, unchanged |
| MOV-6 | Placement uses the interaction grid | Package |
| TFM-7 | The cap becomes a rule | Plan, commit 11 |
| PRB-6 | The sabotage fallback stays | Stays |
| EFF-1 | Merge the two source lists | Plan, commit 2 |
| PRB-5 | Risk 0 rolls normally; the roll uses the displayed chance | Package |
| CMB-6 | The defender with the greatest chance of success; odds are a stub that keeps the first unit for now | Plan, commit 10 |
| MOV-1 | Today's rule matches the original; a candidate for a config option | Plan, commit 11 |
| TFM-5 | Formers on the same project and tile share the countdown | Package |
| CNC-2 | Rank governor candidates by votes | Package |
