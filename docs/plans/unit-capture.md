---
name: Unit capture in combat
overview: "Marine Detachment boarding and alien artifact capture, as terranx.exe does them (terranx-map.md \"Unit capture in combat\"), through one capture mutation shared with the probe team's Subvert Unit."
todos:
  - id: config
    content: Stats board_chance / board_hp_limit, flags captured_on_defeat / attack_not_hostile, condition DefenderIsNativeLife, combat.json boarding_home_base_min_pop, diplomacy artifact_capture_friction; ship them in abilities.json, native_units.json, combat.json, diplomacy.json
    status: completed
  - id: artifact-stats
    content: Artifact 1 HP, flat defense 1 (MaxClamp + MinClamp), collateral_susceptibility × 0, non_combatant
    status: completed
  - id: capture-rules
    content: CaptureRules - boarding threshold and stack, defeated-unit capture and destination, Subvert Unit capture, home base, friction; IsHostileAttack in DiplomaticTransitionRules
    status: completed
  - id: capture-mutations
    content: CaptureMutations - CaptureUnits, CaptureOrDestroyDefeated, ApplyCombatCaptureAftermath; Subvert Unit switches to CaptureUnits
    status: completed
  - id: resolver
    content: CombatResolver boarding check before disengage; every unit the attacker kills goes through CaptureOrDestroyDefeated
    status: completed
  - id: executor
    content: UnitAttackExecutor - no hostile act for attack_not_hostile, captures reported through IUnitOrderWorld::OnCombatCapture; promotion rules
    status: completed
  - id: ui
    content: CaptureNoticeInteraction_t and CaptureNoticeCopy; combat playback shows a captured defender's hit on its pre-fight tile
    status: completed
  - id: binary-checks
    content: Terrain on the artifact's defense and Subvert Unit's home base confirmed in terranx.exe and recorded in the map
    status: completed
  - id: tests-docs
    content: Fixture tests; unit-movement-system.md and player-interaction-system.md; unit-components.md; deviations entries
    status: completed
isProject: false
---

# Unit capture in combat

Two ways a fight hands units to the attacker. The original behaviour, with addresses, is in
[terranx-map.md](../thinker/terranx-map.md), "Unit capture in combat".

- **Boarding (Marine Detachment).** Once the defender has lost `board_hp_limit` of the HP it
  started with, every further round it loses (while alive) rolls `board_chance`. A success
  ends the fight and the attacker takes every unit on the defender's tile.
- **Artifacts.** A combat kill of a `captured_on_defeat` unit becomes a capture when the
  attacker can take it. The unit moves to the attacker's tile.

Both use `CaptureUnits`, which the probe team's Subvert Unit also uses.

## Config

### New ids

| Id | Kind | Meaning |
|---|---|---|
| `board_chance` | unit stat (percent) | Chance per qualifying round that a boarding succeeds. 0 means the unit cannot board. |
| `board_hp_limit` | unit stat (fraction) | Share of the defender's starting HP it must lose before boarding rolls start. The threshold is `floor(startHp × board_hp_limit)`; a threshold of 0 means no boarding. |
| `boarding_home_base_min_pop` | `combat.json` key | A boarded unit is re-homed to the nearest base when the captor owns it and its population is at least this; otherwise it has no home base. |
| `captured_on_defeat` | flag | A combat kill of this unit is a capture when the attacker qualifies. The kill grants no promotion roll. |
| `attack_not_hostile` | flag | Attacking this unit is not a hostile act. Resolved with the unit's tile in context, so conditions can restrict it. |
| `DefenderIsNativeLife` | condition | The combat context's defender is native life (`value`, default true). |
| `artifact_capture_friction` | `diplomacy.json` key | Friction the victim gains against the captor per fight that takes artifacts, when the victim is AI-controlled and not in Vendetta with the captor. |

`0.6667` is the shipped `board_hp_limit`: it reproduces the binary's `floor(2 × HP / 3)` for
every HP the game uses.

### Shipping files

- `config/unit_components/abilities.json`, `Marine_Detachment`: `board_chance` 50 under
  `AllOf` [`SubjectDomain` sea, `DefenderDomain` sea, `DefenderIsNativeLife` false,
  `TargetTileHas` Water, `TargetTileHas` Base absent]; `board_hp_limit` 0.6667.
- `config/native_units.json`, `Alien_Artifact`: `hit_points` 1; `defense` `MaxClamp` 1 and
  `MinClamp` 1 (clamps apply after the morale `AddPercent`, so the rating is a flat 1; the
  tile multiplier still applies, as in the binary); `collateral_susceptibility` × 0;
  `non_combatant`; `captured_on_defeat`; `attack_not_hostile` while `TargetTileHas` Base is
  absent.
- `config/combat.json` (new, `CombatConfig_t` / `CombatConfigParser`): `"boarding_home_base_min_pop": 3`.
- `config/diplomacy.json`: `"artifact_capture_friction": 5`.

## Rules

`CaptureRules` (`include/game/combat/`, `src/game/combat/`). The combat context is the one
`CombatStrength` builds: the defender's tile as target, `pAttacker`, `pDefender`.

- `BoardingThreshold(rAttacker, rDefender)`: HP the defender must lose; unset when
  `board_chance` is 0 or the threshold rounds to 0.
- `BoardingChance(rAttacker, rDefender)`.
- `BoardingCapture(rAttacker, rTile)`: every unit on the tile, occupants then cargo, keeping
  damage, with spent moves. Home base is the nearest base of any owner, kept when the captor
  owns it and it holds `boarding_home_base_min_pop`.
- `DefeatedUnitCapture(rAttacker, rDefeated)`: a capture when `rDefeated` is
  `captured_on_defeat` and the attacker is neither wild native life nor `SingleUse`, standing
  on land or beside a captor carrier with room (`FindCarrierWithRoom`). The unit moves to the
  attacker's tile (onto that carrier on water) at full HP, with no home base and spent moves.
- `SubvertCapture(rActor, rTarget)`: home base is the actor's nearest base; damage kept, moves
  refilled.
- `ArtifactCaptureFriction(captor, victim)`: `artifact_capture_friction` for an AI victim
  outside Vendetta, else 0.

`IsHostileAttack(rAttacker, rDefender)` (`DiplomaticTransitionRules`) is false when the defender
has `attack_not_hostile` in that context. `BombardHostileActs` leaves such targets out.

`MoraleRules`: `KillEarnsPromotion` is false for a `captured_on_defeat` unit;
`PromotionCandidate` returns the attacker after a boarding and nothing for a kill that earns
no promotion.

## Mutations

`CaptureMutations`:

- `CaptureUnits(rPositions, rCaptor, captured)`: `TransferUnitTo` (cargo travels with its
  carrier), the move to the destination and onto the carrier, home base, HP, moves, and a
  cleared order.
- `CaptureOrDestroyDefeated(rPositions, rAttacker, rDefeated)`: `DefeatedUnitCapture`, then
  `CaptureUnits` or `DestroyUnit`. Returns the captured unit's id, unset when it was destroyed.
- `ApplyCombatCaptureAftermath(rGameState, rAttacker, rResult)`: `ArtifactCaptureFriction` for
  units taken on defeat, and a `CaptureNoticeInteraction_t` when the player is the captor or
  the victim.

`ApplySubvertUnit_` calls `CaptureUnits` with `SubvertCapture`.

## Combat flow

- `CombatResolver`: `BoardingThreshold` before the first round (not for artillery). In a round
  the defender loses, `EarlyEnd_` checks `BoardingRollWon_` (which wears the threshold down and
  rolls) before any withdrawal; a won roll ends the rounds, and `Board_` then captures the tile
  and sets `bDefenderBoarded`. `RemoveLosers_` hands a killed defender to `RemoveDefeatedDefender_`, which with
  `ApplyStackCollateral` removes every unit the attacker kills through `CaptureOrDestroyDefeated`.
- `CombatResult_t` gains `bDefenderBoarded`, `bKillEarnsPromotion`, `defenderFactionId` and
  `capturedUnitIds`. A captured defender still counts as destroyed for the outcome.
- `UnitAttackExecutor`: `TryAttack` reports a hostile act only when `IsHostileAttack`. After
  each fight that took units, `IUnitOrderWorld::OnCombatCapture` runs the aftermath.

## UI

`CaptureNoticeCopy` writes the captor's and the victim's battle report, matching the script's
`#WEMARINEDETACH` / `#THEYMARINEDETACH`, `#OURBOOTY1` / `2` and `#THEIRBOOTY1` / `2` / `3`, with
the captured unit's name. `CombatPresentation` draws a defeated unit's hit on its pre-fight
tile, so a captured artifact flashes where it stood.

## Deviations

In `config/deviations/README.md`, under TODO: artifact capture sets no want-to-talk; boarded
units lose their orders; nearest base by Chebyshev distance; collateral hits colony pods,
probes and supply crawlers.
