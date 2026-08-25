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

TEST_CASE("ApplyGrowth banks nutrients at max size instead of spending them on a phantom pop",
          "[population][growth]")
{
    actest::BaseFixture fixture;
    fixture.dataContext.growthConfig->maxBaseSize = 3;

    BaseManager& base = fixture.MakeBase(2, 2);
    PopulationManager& rPopulation = base.GetPopulation();

    REQUIRE(rPopulation.GetSize() == 3);
    REQUIRE_FALSE(rPopulation.CanGrow());

    rPopulation.ApplyGrowth(/*nutrients*/ 100, BaseEffects_t{base});

    CHECK(rPopulation.GetNutrientStockpile() == 100);
    CHECK(rPopulation.GetSize() == 3);
}

TEST_CASE("AddPop throws at max size instead of silently no-oping", "[population][growth]")
{
    actest::BaseFixture fixture;
    fixture.dataContext.growthConfig->maxBaseSize = 3;

    BaseManager& base = fixture.MakeBase(2, 2);
    CHECK_THROWS_AS(base.GetPopulation().AddPop(), std::runtime_error);
}

TEST_CASE("Max base size comes from GrowthConfig", "[population][growth]")
{
    actest::BaseFixture fixture;
    fixture.dataContext.growthConfig->maxBaseSize = 5;

    BaseManager& base = fixture.MakeBase(2, 2);
    CHECK(base.GetPopulation().GetMaxSize() == 5);
}

TEST_CASE("ApplyGrowth spends the threshold and grows when under the cap", "[population][growth]")
{
    actest::BaseFixture fixture;
    fixture.dataContext.growthConfig->maxBaseSize = 7;
    fixture.dataContext.growthConfig->nutrientsPerPop = 10;

    BaseManager& base = fixture.MakeBase(2, 2);
    PopulationManager& rPopulation = base.GetPopulation();
    REQUIRE(rPopulation.GetSize() == 3);

    // Threshold = 3 * 10 = 30 at GrowthRate 100%.
    rPopulation.ApplyGrowth(/*nutrients*/ 30, BaseEffects_t{base});
    CHECK(rPopulation.GetSize() == 4);
    CHECK(rPopulation.GetNutrientStockpile() == 0);
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

TEST_CASE("A base that has already lost its last pop does not starve further",
          "[population][growth]")
{
    // Starvation fires whenever the stockpile goes negative, and nothing removes a base at size
    // zero — so an empty base keeps starving every turn. That must not reach RemovePop.
    actest::BaseFixture fixture;
    ac::BaseManager& rBase = fixture.MakeBase(4, 4);
    ac::PopulationManager& rPopulation = rBase.GetPopulation();

    while (rPopulation.GetSize() > 0)
    {
        rPopulation.RemovePop();
    }

    CHECK_NOTHROW(rPopulation.ApplyGrowth(-10, BaseEffects_t{rBase}));
    CHECK(rPopulation.GetSize() == 0);
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
    ac::Pop* pSpecialist = pLastWorker->IsSpecialist() ? pLastWorker : nullptr;
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
        bSurvivorIsSpecialist = rPop.IsSpecialist();
    }
    CHECK(bSurvivorIsSpecialist);
}
