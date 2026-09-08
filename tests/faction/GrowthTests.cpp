#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/population/calculators/GrowthCalculator.h"
#include "game/population/pop-types/GrowthConfigParser.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"

#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/population/pop-types/Pop.h"
#include "game/map/Tile.h"
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

using namespace ac;
using actest::Active;

TEST_CASE("GrowthRate <= 0 blocks nutrient-threshold growth instead of silently normalizing",
          "[population][growth]")
{
    GrowthConfig_t config;
    config.nutrientsPerPop = 10;

    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);
    actest::EffectPool pool;
    // -100% on the 100 baseline → GrowthRate 0.
    BaseEffects_t effects{base, {
        Active(pool.StatMod(StatId_t::GrowthRate, -100.0, ModifierOp_t::AddPercent), "crush"),
    }};

    CHECK(GrowthCalculator::ComputeNutrientsRequired(config, 3, effects)
          == std::numeric_limits<int>::max());
}

TEST_CASE("ApplyGrowth halves full tanks at max size then deposits net", "[population][growth]")
{
    actest::BaseFixture fixture;
    actest::SetMaxBaseSize(*fixture.dataContext.growthConfig, 3);

    BaseManager& base = fixture.MakeBase(2, 2);
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

    BaseManager& base = fixture.MakeBase(2, 2);
    CHECK_THROWS_AS(base.GetPopulation().AddPop(), std::runtime_error);
}

TEST_CASE("Max base size comes from resolved MaxBaseSize effects", "[population][growth]")
{
    actest::BaseFixture fixture;
    actest::SetMaxBaseSize(*fixture.dataContext.growthConfig, 5);

    BaseManager& base = fixture.MakeBase(2, 2);
    CHECK(base.GetPopulation().GetMaxSize() == 5);
}

TEST_CASE("MaxBaseSize Adds stack with the pop_growth baseline", "[population][growth]")
{
    actest::BaseFixture fixture;
    // Baseline 7 + Hab-Dome-style large Add → classic-ish hard cap.
    actest::SetMaxBaseSize(*fixture.dataContext.growthConfig, 7);
    fixture.dataContext.growthConfig->effects.push_back(
        MakeGrowthBaselineStat(StatId_t::MaxBaseSize, 120.0));
    fixture.pOwnerFaction = std::make_unique<Faction>(
        /*factionId*/ 1, /*bIsPlayerControlled*/ true, fixture.ownerDefinition, fixture.dataContext,
        fixture.map, fixture.settings, actest::k_TestFactionSeed);

    BaseManager& base = fixture.MakeBase(2, 2);
    CHECK(base.GetPopulation().GetMaxSize() == 127);
    CHECK(base.GetPopulation().CanGrow());
}

TEST_CASE("ApplyGrowth grows from a full tank then deposits adjusted net", "[population][growth]")
{
    actest::BaseFixture fixture;
    actest::SetMaxBaseSize(*fixture.dataContext.growthConfig, 7);
    fixture.dataContext.growthConfig->nutrientsPerPop = 10;
    fixture.dataContext.growthConfig->nutrientIntakePerCitizen = 2;

    BaseManager& base = fixture.MakeBase(2, 2);
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
    BaseManager& base = fixture.MakeBase(2, 2);
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
    BaseManager& base = fixture.MakeBase(2, 2);
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
    BaseManager& base = fixture.MakeBase(2, 2);
    GrowthConfig_t config;
    config.nutrientsPerPop = 10;
    CHECK(GrowthCalculator::ComputeNutrientsRequired(config, 3, BaseEffects_t{base}) == 40);
}

TEST_CASE("Losing a pop announces it while it is still valid", "[population][growth]")
{
    // OnPopLost carries only the new size, so an observer holding a Pop& could not tell which
    // pop went and had no point at which the reference was still good. UnitManager has provided
    // that guarantee for units since the lifetime work; this is the same contract for pops.
    actest::BaseFixture fixture;
    ac::BaseManager& rBase = fixture.MakeBase(4, 4);
    ac::PopulationManager& rPopulation = rBase.GetPopulation();

    const ac::Pop* pAnnounced = nullptr;
    int sizeWhenAnnounced = -1;
    int sizeAtPopLost = -1;

    rPopulation.OnPopRemoved.Connect([&](ac::Pop& rPop)
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
    ac::BaseManager& rBase = fixture.MakeBase(4, 4);
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
    ac::BaseManager& rBase = fixture.MakeFactionBase(rFaction, 4, 4);
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
    ac::BaseManager& rBase = fixture.MakeBase(4, 4);
    ac::PopulationManager& rPopulation = rBase.GetPopulation();

    rBase.GetWorkerAssignments().UnassignAll();
    REQUIRE(rPopulation.GetSize() >= 2);

    // The tile has to be worth something, or "productive" ties with "idle" at zero.
    fixture.At(3, 3).SetMoisture(ac::Moisture_t::Wet);

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
    REQUIRE(rBase.GetWorkerAssignments().AssignWorker(*pProductive, &fixture.At(3, 3)));
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
    ac::BaseManager& rBase = fixture.MakeBase(4, 4);
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

TEST_CASE("Falling max size does not trim existing pops", "[population][growth]")
{
    actest::BaseFixture fixture;
    actest::SetMaxBaseSize(*fixture.dataContext.growthConfig, 7);
    BaseManager& base = fixture.MakeBase(2, 2);
    REQUIRE(base.GetPopulation().GetSize() == 3);

    actest::SetMaxBaseSize(*fixture.dataContext.growthConfig, 2);
    // Pool rebuilds from the mutated config on next effects access.
    CHECK(base.GetPopulation().GetSize() == 3);
    CHECK_FALSE(base.GetPopulation().CanGrow());
}

TEST_CASE("Growing never leaves a negative tank", "[population][growth]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(2, 2);
    PopulationManager& rPopulation = base.GetPopulation();
    REQUIRE(rPopulation.GetSize() == 3);

    // Net exactly 0 at size 3 clears the grow gate, but the new citizen's intake is charged
    // after the tank is emptied — the only path that can undershoot zero.
    rPopulation.SetNutrientStockpile(40);
    rPopulation.ApplyGrowth(rPopulation.GetCitizenNutrientIntake(), BaseEffects_t{base});
    CHECK(rPopulation.GetSize() == 4);
    CHECK(rPopulation.GetNutrientStockpile() == 0);
}

TEST_CASE("Founding without an explicit size resolves StartingSize", "[population][growth]")
{
    actest::BaseFixture fixture;
    actest::SetStartingSize(*fixture.dataContext.growthConfig, 2);
    fixture.pOwnerFaction = std::make_unique<Faction>(
        /*factionId*/ 1, /*bIsPlayerControlled*/ true, fixture.ownerDefinition, fixture.dataContext,
        fixture.map, fixture.settings, actest::k_TestFactionSeed);

    BaseManager& base = fixture.MakeBase(2, 2, std::nullopt);
    CHECK(base.GetPopulation().GetSize() == 2);
}

TEST_CASE("A StartingSize that resolves to zero fails loudly at founding",
          "[population][growth]")
{
    actest::BaseFixture fixture;
    actest::SetStartingSize(*fixture.dataContext.growthConfig, 0);
    fixture.pOwnerFaction = std::make_unique<Faction>(
        /*factionId*/ 1, /*bIsPlayerControlled*/ true, fixture.ownerDefinition, fixture.dataContext,
        fixture.map, fixture.settings, actest::k_TestFactionSeed);

    CHECK_THROWS_AS(fixture.MakeBase(2, 2, std::nullopt), std::runtime_error);
}
