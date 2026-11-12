#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/base/BaseManager.h"
#include "game/population/PopulationManager.h"
#include "game/population/GrowthRules.h"
#include "game/population/PopulationRules.h"
#include "game/population/GrowthConfigParser.h"
#include "game/buildings/BuildingConfig.h"
#include "game/buildings/BuildingManager.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/economy/ResourceManager.h"
#include "game/production/ProductionApplyResult.h"
#include "game/production/ProductionManager.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"

#include "game/economy/WorkerAssignmentManager.h"
#include "game/population/Pop.h"
#include "game/map/Tile.h"
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ac;
using actest::Active;

TEST_CASE("GrowthRate <= 0 blocks nutrient-threshold growth instead of silently normalizing",
          "[population][growth]")
{
    GrowthConfig_t config = actest::FixtureGrowthConfig();
    config.nutrientsPerPop = 10;

    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(8, 8);
    actest::EffectPool pool;
    // -100% on the 100 baseline → GrowthRate 0.
    BaseEffects_t effects{base, {
        Active(pool.StatMod(StatId_t::GrowthRate, -100.0, ModifierOp_t::AddPercent), "crush"),
    }};

    CHECK(ComputeNutrientsRequired(config, 3, effects)
          == std::numeric_limits<int>::max());
}

TEST_CASE("Population boom grows with surplus and freezes tanks", "[population][growth]")
{
    actest::BaseFixture fixture;
    actest::SetMaxBaseSize(*fixture.dataContext.growthConfig, 7);
    fixture.dataContext.growthConfig->nutrientsPerPop = 10;
    fixture.dataContext.growthConfig->nutrientIntakePerCitizen = 2;

    BaseManager& base = fixture.MakeBase(8, 4);
    PopulationManager& rPopulation = base.GetPopulation();
    REQUIRE(rPopulation.GetSize() == 3);

    actest::EffectPool pool;
    BaseEffects_t effects{base, {
        Active(pool.RuleFlag(RuleFlagId_t::PopulationBoom), "boom"),
    }};

    rPopulation.SetNutrientStockpile(15);
    // Gross 12 → net 6 at size 3; boom needs net ≥ intake (2).
    rPopulation.ApplyGrowth(12, effects);
    CHECK(rPopulation.GetSize() == 4);
    CHECK(rPopulation.GetNutrientStockpile() == 15);
}

TEST_CASE("Population boom clamps tanks down to the box when above it", "[population][growth]")
{
    GrowthConfig_t config = actest::FixtureGrowthConfig();
    config.nutrientsPerPop = 10;
    config.nutrientIntakePerCitizen = 2;

    GrowthInputs_t inputs;
    inputs.stockpile = 55;
    inputs.netNutrients = 4;
    inputs.boxNow = 40;
    inputs.boxNext = 50;
    inputs.bRoomToGrow = true;
    inputs.bPopulationBoom = true;

    const GrowthOutcome_t outcome = ResolveGrowth(inputs, config);
    CHECK(outcome.bGrows);
    CHECK(outcome.stockpileOnGrowth == 40);
    CHECK(outcome.stockpileAfter == 40);
}

TEST_CASE("Population boom at max size falls through to half-tank", "[population][growth]")
{
    actest::BaseFixture fixture;
    actest::SetMaxBaseSize(*fixture.dataContext.growthConfig, 3);
    fixture.dataContext.growthConfig->nutrientsPerPop = 10;
    fixture.dataContext.growthConfig->nutrientIntakePerCitizen = 2;

    BaseManager& base = fixture.MakeBase(8, 4);
    PopulationManager& rPopulation = base.GetPopulation();
    REQUIRE_FALSE(rPopulation.CanGrow());

    actest::EffectPool pool;
    BaseEffects_t effects{base, {
        Active(pool.RuleFlag(RuleFlagId_t::PopulationBoom), "boom"),
    }};

    rPopulation.SetNutrientStockpile(40);
    rPopulation.ApplyGrowth(8 + rPopulation.GetCitizenNutrientIntake(), effects);
    CHECK(rPopulation.GetSize() == 3);
    CHECK(rPopulation.GetNutrientStockpile() == 20 + 8);
}

TEST_CASE("Population boom needs surplus at least one citizen intake", "[population][growth]")
{
    GrowthConfig_t config = actest::FixtureGrowthConfig();
    config.nutrientsPerPop = 10;
    config.nutrientIntakePerCitizen = 2;

    CHECK_FALSE(MeetsPopulationBoomSurplus(1, config));
    CHECK(MeetsPopulationBoomSurplus(2, config));

    GrowthInputs_t inputs;
    inputs.stockpile = 10;
    inputs.netNutrients = 1;
    inputs.boxNow = 40;
    inputs.boxNext = 50;
    inputs.bRoomToGrow = true;
    inputs.bPopulationBoom = true;

    const GrowthOutcome_t outcome = ResolveGrowth(inputs, config);
    CHECK_FALSE(outcome.bGrows);
    CHECK(outcome.stockpileAfter == 11);
}

TEST_CASE("Near-zero growth pins full tanks without growing", "[population][growth]")
{
    actest::BaseFixture fixture;
    actest::SetMaxBaseSize(*fixture.dataContext.growthConfig, 7);
    fixture.dataContext.growthConfig->nutrientsPerPop = 10;
    fixture.dataContext.growthConfig->nutrientIntakePerCitizen = 2;

    BaseManager& base = fixture.MakeBase(8, 4);
    PopulationManager& rPopulation = base.GetPopulation();
    REQUIRE(rPopulation.GetSize() == 3);

    actest::EffectPool pool;
    BaseEffects_t effects{base, {
        Active(pool.RuleFlag(RuleFlagId_t::NearZeroGrowth), "near_zero"),
    }};

    rPopulation.SetNutrientStockpile(40);
    rPopulation.ApplyGrowth(14, effects);
    CHECK(rPopulation.GetSize() == 3);
    CHECK(rPopulation.GetNutrientStockpile() == 40);
}

TEST_CASE("Near-zero growth deposits normally while below the box", "[population][growth]")
{
    actest::BaseFixture fixture;
    fixture.dataContext.growthConfig->nutrientsPerPop = 10;
    fixture.dataContext.growthConfig->nutrientIntakePerCitizen = 2;

    BaseManager& base = fixture.MakeBase(8, 4);
    PopulationManager& rPopulation = base.GetPopulation();

    actest::EffectPool pool;
    BaseEffects_t effects{base, {
        Active(pool.RuleFlag(RuleFlagId_t::NearZeroGrowth), "near_zero"),
    }};

    rPopulation.SetNutrientStockpile(10);
    rPopulation.ApplyGrowth(14, effects);
    CHECK(rPopulation.GetSize() == 3);
    CHECK(rPopulation.GetNutrientStockpile() == 18);
}

TEST_CASE("Population boom wins over near-zero when surplus allows", "[population][growth]")
{
    GrowthConfig_t config = actest::FixtureGrowthConfig();
    config.nutrientsPerPop = 10;
    config.nutrientIntakePerCitizen = 2;

    GrowthInputs_t inputs;
    inputs.stockpile = 40;
    inputs.netNutrients = 4;
    inputs.boxNow = 40;
    inputs.boxNext = 50;
    inputs.bRoomToGrow = true;
    inputs.bPopulationBoom = true;
    inputs.bNearZeroGrowth = true;

    const GrowthOutcome_t outcome = ResolveGrowth(inputs, config);
    CHECK(outcome.bGrows);
    CHECK(outcome.stockpileAfter == 40);
}

TEST_CASE("ApplyGrowth halves full tanks at max size then deposits net", "[population][growth]")
{
    actest::BaseFixture fixture;
    actest::SetMaxBaseSize(*fixture.dataContext.growthConfig, 3);

    BaseManager& base = fixture.MakeBase(8, 4);
    PopulationManager& rPopulation = base.GetPopulation();

    REQUIRE(rPopulation.GetSize() == 3);
    REQUIRE_FALSE(rPopulation.CanGrow());

    // size 3 → required (3+1)*10 = 40; ApplyGrowth takes gross and subtracts intake.
    rPopulation.SetNutrientStockpile(40);
    const int gross = 8 + rPopulation.GetCitizenNutrientIntake();
    rPopulation.ApplyGrowth(gross, BaseEffects_t{base});

    CHECK(rPopulation.GetNutrientStockpile() == 20 + 8); // half of 40, then +8
    CHECK(rPopulation.GetSize() == 3);
}

TEST_CASE("AddPop throws at max size instead of silently no-oping", "[population][growth]")
{
    actest::BaseFixture fixture;
    actest::SetMaxBaseSize(*fixture.dataContext.growthConfig, 3);

    BaseManager& base = fixture.MakeBase(8, 4);
    CHECK_THROWS_AS(base.GetPopulation().AddPop(), std::runtime_error);
}

TEST_CASE("Max base size comes from resolved MaxBaseSize effects", "[population][growth]")
{
    actest::BaseFixture fixture;
    actest::SetMaxBaseSize(*fixture.dataContext.growthConfig, 5);

    BaseManager& base = fixture.MakeBase(8, 4);
    CHECK(base.GetPopulation().GetMaxSize() == 5);
}

TEST_CASE("Hab Complex raises MaxBaseSize by 7 over the pop_growth baseline",
          "[population][growth]")
{
    actest::FactionFixture fixture;
    actest::SetMaxBaseSize(*fixture.dataContext.growthConfig, 7);
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 8, 4);
    CHECK(base.GetPopulation().GetMaxSize() == 7);

    base.GetBuildingManager().AddBuilding("Hab_Complex");
    CHECK(base.GetPopulation().GetMaxSize() == 14);

    base.GetBuildingManager().AddBuilding("Habitation_Dome");
    CHECK(base.GetPopulation().GetMaxSize() == 127);
}

TEST_CASE("MaxBaseSize Adds stack with the pop_growth baseline", "[population][growth]")
{
    actest::BaseFixture fixture;
    // Baseline 7 + Hab-Dome-style large Add → classic-ish hard cap.
    actest::SetMaxBaseSize(*fixture.dataContext.growthConfig, 7);
    fixture.dataContext.growthConfig->effects.push_back(
        actest::GrowthBaselineStat(StatId_t::MaxBaseSize, 120.0));

    BaseManager& base = fixture.MakeBase(8, 4);
    CHECK(base.GetPopulation().GetMaxSize() == 127);
    CHECK(base.GetPopulation().CanGrow());
}

TEST_CASE("ApplyGrowth grows from a full tank then deposits adjusted net", "[population][growth]")
{
    actest::BaseFixture fixture;
    actest::SetMaxBaseSize(*fixture.dataContext.growthConfig, 7);
    fixture.dataContext.growthConfig->nutrientsPerPop = 10;
    fixture.dataContext.growthConfig->nutrientIntakePerCitizen = 2;

    BaseManager& base = fixture.MakeBase(8, 4);
    PopulationManager& rPopulation = base.GetPopulation();
    REQUIRE(rPopulation.GetSize() == 3);

    // required = (3+1)*10 = 40; gross 14 → net 8 at size 3 (intake 2×3).
    rPopulation.SetNutrientStockpile(40);
    rPopulation.ApplyGrowth(/*gross*/ 14, BaseEffects_t{base});
    CHECK(rPopulation.GetSize() == 4);
    // Emptied, then deposit 8 - 2 intake for the new citizen, capped at new required 50.
    CHECK(rPopulation.GetNutrientStockpile() == 6);
}

TEST_CASE("ApplyGrowth does not grow in the same pass that fills the tank", "[population][growth]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(8, 4);
    PopulationManager& rPopulation = base.GetPopulation();
    REQUIRE(rPopulation.GetSize() == 3);

    rPopulation.SetNutrientStockpile(30);
    rPopulation.ApplyGrowth(20 + rPopulation.GetCitizenNutrientIntake(), BaseEffects_t{base});
    CHECK(rPopulation.GetSize() == 3);
    CHECK(rPopulation.GetNutrientStockpile() == 40); // capped at required
}

TEST_CASE("Full tank with negative net does not grow", "[population][growth]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(8, 4);
    PopulationManager& rPopulation = base.GetPopulation();
    rPopulation.SetNutrientStockpile(40);
    // Gross that yields net -5 after size-3 intake.
    rPopulation.ApplyGrowth(-5 + rPopulation.GetCitizenNutrientIntake(), BaseEffects_t{base});
    CHECK(rPopulation.GetSize() == 3);
    CHECK(rPopulation.GetNutrientStockpile() == 35);
}

TEST_CASE("Threshold at size 3 uses size+1 rows", "[population][growth]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(8, 4);
    GrowthConfig_t config = actest::FixtureGrowthConfig();
    config.nutrientsPerPop = 10;
    CHECK(ComputeNutrientsRequired(config, 3, BaseEffects_t{base}) == 40);
}

TEST_CASE("Losing a pop announces it while it is still valid", "[population][growth]")
{
    // OnPopLost carries only the new size, so an observer holding a Pop& could not tell which
    // pop went and had no point at which the reference was still good. UnitManager has provided
    // that guarantee for units since the lifetime work; this is the same contract for pops.
    actest::BaseFixture fixture;
    ac::BaseManager& rBase = fixture.MakeBase(8, 8);
    ac::PopulationManager& rPopulation = rBase.GetPopulation();

    const ac::Pop* pAnnounced = nullptr;
    int sizeWhenAnnounced = -1;
    int sizeAtPopLost = -1;

    rPopulation.OnPopRemoved.Connect([&](const ac::Pop& rPop)
    {
        pAnnounced = &rPop;
        // Still present: the observer can read it, not just learn that something went.
        sizeWhenAnnounced = rPopulation.GetSize();
        CHECK(std::string(rPop.GetPopType()).empty() == false);
    });
    rPopulation.OnPopLost.Connect([&](int newSize) { sizeAtPopLost = newSize; });

    const int before = rPopulation.GetSize();
    REQUIRE(before > 0);

    rPopulation.RemovePop();

    CHECK(pAnnounced != nullptr);
    CHECK(sizeWhenAnnounced == before);
    CHECK(sizeAtPopLost == before - 1);
    CHECK(rPopulation.GetSize() == before - 1);
}

TEST_CASE("Removing a pop from an empty base is a caller bug, not a no-op",
          "[population][growth]")
{
    actest::BaseFixture fixture;
    ac::BaseManager& rBase = fixture.MakeBase(8, 8);
    ac::PopulationManager& rPopulation = rBase.GetPopulation();

    while (rPopulation.GetSize() > 0)
    {
        rPopulation.RemovePop();
    }
    CHECK_THROWS_AS(rPopulation.RemovePop(), std::runtime_error);
}

TEST_CASE("Losing the last pop razes the base so it is not starved again",
          "[population][growth][raze]")
{
    // Rule: size 0 razes immediately via OnPopLost. ApplyBaseGrowth only walks live bases,
    // so a starved-out base never receives another OnStarvation. See docs/game-rules-decisions.md.
    actest::FactionFixture fixture;
    ac::Faction& rFaction = fixture.MakeFaction();
    ac::BaseManager& rBase = fixture.MakeFactionBase(rFaction, 8, 8);
    const ac::BaseId_t baseId = rBase.GetBaseId();
    ac::PopulationManager& rPopulation = rBase.GetPopulation();

    while (rPopulation.GetSize() > 0)
    {
        rPopulation.RemovePop();
    }

    CHECK(rBase.IsRazed());
    CHECK(rFaction.FindBase(baseId) == nullptr);
    CHECK(rFaction.GetBaseCount() == 0);
    CHECK_NOTHROW(rFaction.ApplyBaseGrowth());
}

TEST_CASE("A shrinking base loses its least productive pop", "[population][growth]")
{
    // Rule: specialists last; within a group, the pop producing the least total resource.
    // See docs/game-rules-decisions.md. Previously it was always the most recently added,
    // which could take a talent working a good tile while an idle worker sat beside it.
    actest::BaseFixture fixture;
    ac::BaseManager& rBase = fixture.MakeBase(8, 8);
    ac::PopulationManager& rPopulation = rBase.GetPopulation();

    rBase.GetWorkerAssignments().UnassignAll();
    REQUIRE(rPopulation.GetSize() >= 2);

    // The tile has to be worth something, or "productive" ties with "idle" at zero.
    fixture.At(8, 6).SetMoisture(ac::Moisture_t::Wet);

    // One pop works a tile; the rest are idle and therefore worth nothing. It must be the
    // *last* pop: the rule this replaced always took the most recently added, so making the
    // first pop productive would pass under either rule.
    ac::Pop* pProductive = nullptr;
    for (ac::Pop& rPop : rPopulation.Pops())
    {
        if (rPop.IsWorker())
        {
            pProductive = &rPop;
        }
    }
    REQUIRE(pProductive != nullptr);
    REQUIRE(rBase.GetWorkerAssignments().AssignWorker(*pProductive, &fixture.At(8, 6)));
    REQUIRE(pProductive->GetTile() != nullptr);

    // Every idle pop goes before the one that is actually producing.
    const int idleCount = rPopulation.GetSize() - 1;
    for (int i = 0; i < idleCount; ++i)
    {
        rPopulation.RemovePop();
        CHECK(pProductive->GetTile() != nullptr);
    }
    CHECK(rPopulation.GetSize() == 1);
}

TEST_CASE("Specialists are the last pops lost", "[population][growth]")
{
    actest::BaseFixture fixture;
    ac::BaseManager& rBase = fixture.MakeBase(8, 8);
    ac::PopulationManager& rPopulation = rBase.GetPopulation();

    rBase.GetWorkerAssignments().UnassignAll();
    REQUIRE(rPopulation.GetSize() >= 2);

    // Convert the *last* pop to a specialist — the one the previous rule would have taken
    // first — and leave every worker idle, so on raw output the specialist is the more valuable
    // pop. It must still be taken last.
    ac::Pop* pLastWorker = nullptr;
    for (ac::Pop& rPop : rPopulation.Pops())
    {
        if (rPop.IsWorker())
        {
            pLastWorker = &rPop;
        }
    }
    REQUIRE(pLastWorker != nullptr);
    rBase.ConvertPop(*pLastWorker, "Doctor");
    ac::Pop* pSpecialist = pLastWorker->IsPlayerChoiceType() ? pLastWorker : nullptr;
    if (!pSpecialist)
    {
        SUCCEED("fixture pop types offer no specialist to convert to");
        return;
    }

    const int workerCount = rPopulation.GetSize() - 1;
    for (int i = 0; i < workerCount; ++i)
    {
        rPopulation.RemovePop();
    }
    REQUIRE(rPopulation.GetSize() == 1);
    bool bSurvivorIsSpecialist = false;
    for (const ac::Pop& rPop : rPopulation.Pops())
    {
        bSurvivorIsSpecialist = rPop.IsPlayerChoiceType();
    }
    CHECK(bSurvivorIsSpecialist);
}

TEST_CASE("SelectPopToRemove picks an idle worker before a producing one", "[population][growth]")
{
    actest::BaseFixture fixture;
    ac::BaseManager& rBase = fixture.MakeBase(8, 8);
    ac::PopulationManager& rPopulation = rBase.GetPopulation();
    rBase.GetWorkerAssignments().UnassignAll();
    fixture.At(8, 6).SetMoisture(ac::Moisture_t::Wet);

    ac::Pop* pProductive = nullptr;
    for (ac::Pop& rPop : rPopulation.Pops())
    {
        if (rPop.IsWorker())
        {
            pProductive = &rPop;
        }
    }
    REQUIRE(pProductive != nullptr);
    REQUIRE(rBase.GetWorkerAssignments().AssignWorker(*pProductive, &fixture.At(8, 6)));

    const ac::Pop& rChosen = ac::SelectPopToRemove(rBase);
    CHECK(&rChosen != pProductive);
    CHECK(rChosen.GetTile() == nullptr);
}

TEST_CASE("Falling max size does not trim existing pops", "[population][growth]")
{
    actest::BaseFixture fixture;
    actest::SetMaxBaseSize(*fixture.dataContext.growthConfig, 7);
    BaseManager& base = fixture.MakeBase(8, 4);
    REQUIRE(base.GetPopulation().GetSize() == 3);

    actest::SetMaxBaseSize(*fixture.dataContext.growthConfig, 2);
    // Pool rebuilds from the mutated config on next effects access.
    CHECK(base.GetPopulation().GetSize() == 3);
    CHECK_FALSE(base.GetPopulation().CanGrow());
}

TEST_CASE("Threshold growth can leave tanks negative for next turn", "[population][growth]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(8, 4);
    PopulationManager& rPopulation = base.GetPopulation();
    REQUIRE(rPopulation.GetSize() == 3);

    // Net exactly 0 at size 3 grows; the new citizen's intake is charged after empty.
    rPopulation.SetNutrientStockpile(40);
    rPopulation.ApplyGrowth(rPopulation.GetCitizenNutrientIntake(), BaseEffects_t{base});
    CHECK(rPopulation.GetSize() == 4);
    CHECK(rPopulation.GetNutrientStockpile() == -2);
}

TEST_CASE("Founding without an explicit size resolves StartingSize", "[population][growth]")
{
    actest::BaseFixture fixture;
    actest::SetStartingSize(*fixture.dataContext.growthConfig, 2);

    BaseManager& base = fixture.MakeBase(8, 4, std::nullopt);
    CHECK(base.GetPopulation().GetSize() == 2);
}

TEST_CASE("A StartingSize that resolves to zero fails loudly at founding",
          "[population][growth]")
{
    actest::BaseFixture fixture;
    actest::SetStartingSize(*fixture.dataContext.growthConfig, 0);

    CHECK_THROWS_AS(fixture.MakeBase(8, 4, std::nullopt), std::runtime_error);
}

namespace
{

GrowthInputs_t GrowthInputs_(int stockpile, int net, bool bRoomToGrow)
{
    GrowthInputs_t inputs;
    inputs.stockpile = stockpile;
    inputs.netNutrients = net;
    inputs.boxNow = 40;
    inputs.boxNext = 50;
    inputs.bRoomToGrow = bRoomToGrow;
    return inputs;
}

} // namespace

TEST_CASE("ResolveGrowth: a full box at the population limit keeps half", "[population][growth]")
{
    GrowthConfig_t config = actest::FixtureGrowthConfig();
    const GrowthOutcome_t outcome = ResolveGrowth(GrowthInputs_(40, 4, false), config);
    CHECK_FALSE(outcome.bGrows);
    CHECK_FALSE(outcome.bStarves);
    CHECK(outcome.stockpileAfter == 24);
}

TEST_CASE("ResolveGrowth: growth empties the box and the new citizen eats", "[population][growth]")
{
    GrowthConfig_t config = actest::FixtureGrowthConfig();
    config.nutrientIntakePerCitizen = 2;
    const GrowthOutcome_t outcome = ResolveGrowth(GrowthInputs_(40, 4, true), config);
    CHECK(outcome.bGrows);
    CHECK(outcome.stockpileOnGrowth == 0);
    CHECK(outcome.stockpileAfter == 2);

    // Post-birth intake can leave tanks negative for next turn.
    CHECK(ResolveGrowth(GrowthInputs_(40, 1, true), config).stockpileAfter == -1);
}

TEST_CASE("ResolveGrowth: starvation only when tanks are already negative", "[population][growth]")
{
    GrowthConfig_t config = actest::FixtureGrowthConfig();
    const GrowthOutcome_t depleting = ResolveGrowth(GrowthInputs_(3, -5, true), config);
    CHECK_FALSE(depleting.bStarves);
    CHECK(depleting.stockpileAfter == -2);

    const GrowthOutcome_t starving = ResolveGrowth(GrowthInputs_(-1, -5, true), config);
    CHECK(starving.bStarves);
    CHECK_FALSE(starving.bGrows);
    CHECK(starving.stockpileAfter == -5);
}

TEST_CASE("PrimeNutrientTanks lifts negatives and primes empty tanks", "[population][growth]")
{
    CHECK(PrimeNutrientTanks(-3, 2) == 0);
    CHECK(PrimeNutrientTanks(0, -4) == -1);
    CHECK(PrimeNutrientTanks(5, -4) == 5);
    CHECK(PrimeNutrientTanks(-2, -1) == -2);
}

TEST_CASE("Empty tanks with negative net starve the same turn after priming",
          "[population][growth]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(8, 4);
    PopulationManager& rPopulation = base.GetPopulation();
    const int sizeBefore = rPopulation.GetSize();
    rPopulation.SetNutrientStockpile(0);
    rPopulation.PrimeNutrientTanks(0);
    REQUIRE(rPopulation.GetNutrientStockpile() == -1);
    const int net = -rPopulation.GetCitizenNutrientIntake();

    rPopulation.ApplyGrowth(0, BaseEffects_t{base});
    CHECK(rPopulation.GetSize() == sizeBefore - 1);
    CHECK(rPopulation.GetNutrientStockpile() == net);
}

TEST_CASE("Positive tanks that go negative starve on the next growth pass",
          "[population][growth]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(8, 4);
    PopulationManager& rPopulation = base.GetPopulation();
    const int sizeBefore = rPopulation.GetSize();
    rPopulation.SetNutrientStockpile(3);
    const int intake = rPopulation.GetCitizenNutrientIntake();

    rPopulation.ApplyGrowth(intake - 5, BaseEffects_t{base});
    CHECK(rPopulation.GetSize() == sizeBefore);
    CHECK(rPopulation.GetNutrientStockpile() == -2);

    rPopulation.ApplyGrowth(intake, BaseEffects_t{base});
    CHECK(rPopulation.GetSize() == sizeBefore - 1);
}

TEST_CASE("ResolveGrowth: the deposit is capped at the box", "[population][growth]")
{
    GrowthConfig_t config = actest::FixtureGrowthConfig();
    const GrowthOutcome_t outcome = ResolveGrowth(GrowthInputs_(10, 50, true), config);
    CHECK_FALSE(outcome.bGrows);
    CHECK(outcome.stockpileAfter == 40);
}

TEST_CASE("A base at its size cap grows the same turn its Hab Complex completes",
          "[population][growth][production]")
{
    actest::EffectPool pool;
    actest::FactionFixture fixture;
    actest::SetMaxBaseSize(*fixture.dataContext.growthConfig, 3);
    BuildingConfig_t hab;
    hab.id = "test_hab_complex";
    hab.name = "Test Hab Complex";
    hab.effects = {pool.StatMod(StatId_t::MaxBaseSize, 1, ModifierOp_t::Add,
                                EffectScope_t::ThisBase)};
    // Assigned before any base exists, so no building manager holds the replaced configs.
    std::vector<BuildingConfig_t> buildings = fixture.buildings().GetAll();
    buildings.push_back(hab);
    fixture.buildings().Assign(std::move(buildings));

    BaseManager& base = fixture.MakeFactionBase(fixture.MakeFaction(), 8, 4);
    PopulationManager& rPopulation = base.GetPopulation();
    REQUIRE(rPopulation.GetSize() == 3);
    REQUIRE_FALSE(rPopulation.CanGrow());
    rPopulation.SetNutrientStockpile(rPopulation.GetNutrientsRequired(base.GetBaseEffects()));
    base.GetResources().AddResource(StatId_t::Nutrients, rPopulation.GetCitizenNutrientIntake());

    const BuildingConfig_t& rHab = fixture.buildings().Get("test_hab_complex");
    base.GetProduction().SetProduction(&rHab, base.GetBaseEffects());
    base.GetProduction().SetMineralStockpile(base.GetMineralCost());
    base.ApplyProduction();
    REQUIRE(base.GetProduction().TryCompleteReadyAtTurnStart().kind
            == ProductionApplyKind_t::Completed);
    CHECK(rPopulation.GetSize() == 3);

    base.ApplyGrowth();
    CHECK(rPopulation.GetSize() == 4);
}
