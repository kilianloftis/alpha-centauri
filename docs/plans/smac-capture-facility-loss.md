# SMAC capture facility loss

On unit capture, destroy each eligible facility independently with probability 1/3
(`!(game_rand() % 3)`), matching `capture_base` / Thinker `mod_capture_base`. Drop the
min/max count and shuffle-take-N model.

## Target behaviour

After a successful unit capture (not probe mind control, not liberation recapture):

1. For each built regular facility except Headquarters and any facility whose loss would
   leave the base unable to occupy its tile, destroy it when
   `RollExclusiveMax(3, rng) == 0` (~1/3). Secret projects are never in this loop.
2. Always clear Headquarters on transfer (`TransferBaseTo` / `DropHeadquarters_` — already).
3. Also destroy Recreation Commons and Recycling Tanks if still present (vanilla SMAC after
   the random loop; Thinker only skips this behind `facility_capture_fix`).
4. Probe mind control: no random facility loss (today already bypasses `ApplyCapture_`).
5. Liberation: when the capturer is the base’s former owner (`OccupierWindow().formerOwner`),
   skip the random loop and the Commons/Tanks wipe; HQ still drops on transfer if needed.

Native raid and riot `DestroyFacility` keep shuffle-take-N — those are different SMAC paths.

## Rules / mutations

- [`CaptureOutcome_t`](include/game/base/BaseConquestRules.h): remove
  `minFacilitiesDestroyed` / `maxFacilitiesDestroyed`. Add `bRollCaptureFacilityLoss`.
  Keep `facilityCandidates` (eligible list for the mutation loop).
- [`PlanCapture`](src/game/base/BaseConquestRules.cpp): build candidates with HQ and secret
  projects excluded (`FacilityExclusions_t` +
  [`CollectDestroyableFacilities`](src/game/buildings/BuildingRules.cpp), which also omits
  would-raze facilities). Set `bRollCaptureFacilityLoss` false for liberation; true otherwise.
- [`DestroyCaptureFacilities_`](src/game/base/BaseConquestMutations.cpp): if
  `bRollCaptureFacilityLoss`, per candidate `RollExclusiveMax(3) == 0` → destroy; then
  force-destroy Commons and Recycling Tanks by id if present. Count →
  `result.facilitiesDestroyed`. Keep local `DestroyRandomFacilities_` for native raid only.

## Config / effects cleanup

Delete `CaptureFacilitiesDestroyedMin` and `CaptureFacilitiesDestroyedMaxPercent` end-to-end:

- Enum + wire strings in [`EffectEnums.h`](include/game/effects/EffectEnums.h)
- Baseline Adds in [`config/base_conquest.json`](config/base_conquest.json) and
  [`tests/fixtures/base_conquest.json`](tests/fixtures/base_conquest.json)
- `test_capture_facility_shield` clamps in buildings fixtures
- [`tests/GameFixtures.cpp`](tests/GameFixtures.cpp) / [`BaseConquestTests`](tests/game/BaseConquestTests.cpp)
  setups that force min/max
- Validation static_asserts for those stats

No replacement Additive stats. Chance is fixed SMAC 1/3 via `RollExclusiveMax(3)`.

## Docs

- [`docs/architecture/unit-movement-system.md`](docs/architecture/unit-movement-system.md) —
  replace min/max percent wording with independent 1/3 + exemptions + liberation skip.
- [`docs/architecture/effects-system.md`](docs/architecture/effects-system.md) — drop the two
  capture facility stats from the overview.
- [`docs/thinker/terranx-map.md`](docs/thinker/terranx-map.md) — short “Capture facility loss”
  under base transfer: loop `1..SP_ID_First-1`, 1/3, Pressure Dome skip, probe/recapture skip,
  HQ after, Commons/Tanks wipe.

## Tests

- Seeded multi-facility capture: document expected destroy count for a fixed `mt19937` seed.
- Pressure Dome on a sea base survives the random loop.
- Liberation (former owner) destroys none via the random loop.
- Mind control leaves facilities (except HQ drop on transfer).
- Remove the `test_capture_facility_shield` / min-max clamp case and PlanCapture min≤max check.
- HQ-always-lost case keeps working without min/max = 0 setup.

## Files

- `include/game/base/BaseConquestRules.h`, `src/game/base/BaseConquestRules.cpp`
- `src/game/base/BaseConquestMutations.cpp`
- `include/game/buildings/FacilityExclusions.h`, `src/game/buildings/BuildingRules.cpp`
- `include/game/effects/EffectEnums.h`
- `config/base_conquest.json`, fixtures, `BaseConquestTests.cpp`, `GameFixtures.cpp`,
  `ValidationTests.cpp`
- Architecture + terranx-map docs above
