#include "GameFixtures.h"
#include "TestHelpers.h"

#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/population/calculators/GrowthCalculator.h"
#include "game/population/pop-types/GrowthConfigParser.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"

#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <stdexcept>

using namespace ac;
using actest::Active;

TEST_CASE("GrowthRate <= 0 blocks nutrient-threshold growth instead of silently normalizing",
          "[population][growth]")
{
    GrowthConfig_t config;
    config.nutrientsPerPop = 10;

    actest::EffectPool pool;
    // -100% on the 100 baseline → GrowthRate 0.
    BaseEffects_t effects{{
        Active(pool.StatMod(StatId::GrowthRate, -100.0, ModifierOp::AddPercent), "crush"),
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

    rPopulation.ApplyGrowth(/*nutrients*/ 100, {});

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
    rPopulation.ApplyGrowth(/*nutrients*/ 30, {});
    CHECK(rPopulation.GetSize() == 4);
    CHECK(rPopulation.GetNutrientStockpile() == 0);
}
