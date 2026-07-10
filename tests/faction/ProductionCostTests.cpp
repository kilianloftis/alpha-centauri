#include "game/faction/base/production/ProductionCostCalculator.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/IConstructable.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"

#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>
#include <string>

using namespace ac;
using actest::Active;

namespace
{

struct StubConstructable : IConstructable
{
    std::string id = "stub_item";
    std::string name = "Stub Item";
    int baseCost = 10;

    const char* GetId() const override { return id.c_str(); }
    const std::string& GetName() const override { return name; }
    int GetBaseCost() const override { return baseCost; }
};

BaseEffects_t WithCostPercent(actest::EffectPool& rPool, double addPercent)
{
    return BaseEffects_t{{
        Active(rPool.StatMod(StatId::CostMultiplier, addPercent, ModifierOp::AddPercent), "industry"),
    }};
}

} // namespace

TEST_CASE("Production cost applies CostMultiplier effects like GrowthRate", "[production][cost]")
{
    actest::EffectPool pool;

    SECTION("no CostMultiplier effects is normal rate (base_cost * minerals_per_row)")
    {
        CHECK(ProductionCostCalculator::ComputeCost(10, BaseEffects_t{}) == 100);
    }

    SECTION("negative CostMultiplier AddPercent reduces cost (Industry +)")
    {
        CHECK(ProductionCostCalculator::ComputeCost(10, WithCostPercent(pool, -10.0)) == 90);
        CHECK(ProductionCostCalculator::ComputeCost(10, WithCostPercent(pool, -50.0)) == 50);
    }

    SECTION("positive CostMultiplier AddPercent raises cost (Industry -)")
    {
        CHECK(ProductionCostCalculator::ComputeCost(10, WithCostPercent(pool, 10.0)) == 110);
        CHECK(ProductionCostCalculator::ComputeCost(10, WithCostPercent(pool, 30.0)) == 130);
    }

    SECTION("CostMultiplier that zeroes the cost still floors at 1")
    {
        CHECK(ProductionCostCalculator::ComputeCost(10, WithCostPercent(pool, -100.0)) == 1);
    }
}

TEST_CASE("ProductionManager resolves cost from base effects", "[production][cost]")
{
    ProductionManager production;
    StubConstructable item;
    production.SetProduction(&item);

    actest::EffectPool pool;
    CHECK(production.GetMineralCost(BaseEffects_t{}) == 100);
    CHECK(production.GetMineralCost(WithCostPercent(pool, -20.0)) == 80);

    CHECK(production.ApplyProduction(50, BaseEffects_t{}).empty());
    CHECK(production.GetMineralStockpile() == 50);
    CHECK(production.HasProduction());

    CHECK(production.ApplyProduction(50, BaseEffects_t{}) == "stub_item");
    CHECK_FALSE(production.HasProduction());
}
