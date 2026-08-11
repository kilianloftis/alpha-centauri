#include "game/faction/base/production/ProductionCostCalculator.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/IConstructable.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"

#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>
#include <string>

using namespace ac;

namespace
{
// The shipped rates; these tests are about the multiplier, not the config.
const ProductionConfig_t k_TestConfig{};
} // namespace
using actest::Active;

namespace
{

struct StubConstructable : IConstructable
{
    std::string id = "stub_item";
    std::string name = "Stub Item";
    int baseCost = 10;

    StubConstructable() = default;
    StubConstructable(std::string itemId, std::string itemName)
        : id(std::move(itemId)), name(std::move(itemName)) {}

    const std::string& GetId() const override { return id; }
    const std::string& GetName() const override { return name; }
    int GetBaseCost() const override { return baseCost; }
};

BaseEffects_t WithCostPercent(actest::EffectPool& rPool, double addPercent)
{
    return BaseEffects_t{{
        Active(rPool.StatMod(StatId_t::CostMultiplier, addPercent, ModifierOp_t::AddPercent), "industry"),
    }};
}

} // namespace

TEST_CASE("Production cost applies CostMultiplier effects like GrowthRate", "[production][cost]")
{
    actest::EffectPool pool;

    SECTION("no CostMultiplier effects is normal rate (base_cost * minerals_per_row)")
    {
        CHECK(ProductionCostCalculator::ComputeCost(10, k_TestConfig.mineralsPerRow, BaseEffects_t{}) == 100);
    }

    SECTION("negative CostMultiplier AddPercent reduces cost (Industry +)")
    {
        CHECK(ProductionCostCalculator::ComputeCost(10, k_TestConfig.mineralsPerRow, WithCostPercent(pool, -10.0)) == 90);
        CHECK(ProductionCostCalculator::ComputeCost(10, k_TestConfig.mineralsPerRow, WithCostPercent(pool, -50.0)) == 50);
    }

    SECTION("positive CostMultiplier AddPercent raises cost (Industry -)")
    {
        CHECK(ProductionCostCalculator::ComputeCost(10, k_TestConfig.mineralsPerRow, WithCostPercent(pool, 10.0)) == 110);
        CHECK(ProductionCostCalculator::ComputeCost(10, k_TestConfig.mineralsPerRow, WithCostPercent(pool, 30.0)) == 130);
    }

    SECTION("CostMultiplier that zeroes the cost still floors at 1")
    {
        CHECK(ProductionCostCalculator::ComputeCost(10, k_TestConfig.mineralsPerRow, WithCostPercent(pool, -100.0)) == 1);
    }
}

TEST_CASE("ProductionManager resolves cost from base effects", "[production][cost]")
{
    ProductionManager production(k_TestConfig);
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

// Retooling. Switching away from what the base started the turn on forfeits half the minerals
// already spent, once more than the threshold has accumulated; switching back is free; switching
// on to a third item pays again. See docs/game-rules-decisions.md.
TEST_CASE("Retooling forfeits half the minerals spent past the threshold", "[production][retool]")
{
    const StubConstructable itemA{"a", "A"};
    const StubConstructable itemB{"b", "B"};

    ProductionManager production(k_TestConfig);
    production.SetProduction(&itemA);
    production.ApplyProduction(0, BaseEffects_t{}); // marks A as this turn's original
    production.SetMineralStockpile(40);

    production.SetProduction(&itemB);
    CHECK(production.GetMineralStockpile() == 20);
}

TEST_CASE("Retooling is free at or below the threshold", "[production][retool]")
{
    const StubConstructable itemA{"a", "A"};
    const StubConstructable itemB{"b", "B"};

    ProductionManager production(k_TestConfig);
    production.SetProduction(&itemA);
    production.ApplyProduction(0, BaseEffects_t{});
    production.SetMineralStockpile(k_TestConfig.retoolPenaltyThreshold);

    production.SetProduction(&itemB);
    CHECK(production.GetMineralStockpile() == k_TestConfig.retoolPenaltyThreshold);
}

TEST_CASE("Switching back to the turn's original item is free", "[production][retool]")
{
    const StubConstructable itemA{"a", "A"};
    const StubConstructable itemB{"b", "B"};

    ProductionManager production(k_TestConfig);
    production.SetProduction(&itemA);
    production.ApplyProduction(0, BaseEffects_t{});
    production.SetMineralStockpile(40);

    production.SetProduction(&itemB);
    REQUIRE(production.GetMineralStockpile() == 20);

    // Back to A: no second charge, and no refund either.
    production.SetProduction(&itemA);
    CHECK(production.GetMineralStockpile() == 20);
}

TEST_CASE("Switching on to a third item pays again", "[production][retool]")
{
    const StubConstructable itemA{"a", "A"};
    const StubConstructable itemB{"b", "B"};
    const StubConstructable itemC{"c", "C"};

    ProductionManager production(k_TestConfig);
    production.SetProduction(&itemA);
    production.ApplyProduction(0, BaseEffects_t{});
    production.SetMineralStockpile(40);

    production.SetProduction(&itemB);
    REQUIRE(production.GetMineralStockpile() == 20);
    production.SetProduction(&itemC);
    CHECK(production.GetMineralStockpile() == 10);
}

TEST_CASE("The turn's original item follows production, turn by turn", "[production][retool]")
{
    // A switch made last turn becomes this turn's baseline: going back to what you built the
    // turn before is a retool like any other.
    const StubConstructable itemA{"a", "A"};
    const StubConstructable itemB{"b", "B"};

    ProductionManager production(k_TestConfig);
    production.SetProduction(&itemA);
    production.ApplyProduction(0, BaseEffects_t{});
    production.SetMineralStockpile(40);
    production.SetProduction(&itemB);
    REQUIRE(production.GetMineralStockpile() == 20);

    // Next turn: B is now the original.
    production.ApplyProduction(0, BaseEffects_t{});
    production.SetMineralStockpile(40);
    production.SetProduction(&itemA);
    CHECK(production.GetMineralStockpile() == 20);
}

TEST_CASE("Null turn original skips retool until ApplyProduction stamps one",
          "[production][retool]")
{
    // Fresh manager (and founding mineral banks) have no turn original — queue/switch free
    // until ApplyProduction banks with something queued.
    const StubConstructable itemA{"a", "A"};
    const StubConstructable itemB{"b", "B"};

    ProductionManager production(k_TestConfig);
    production.SetMineralStockpile(40);

    production.SetProduction(&itemA);
    CHECK(production.GetMineralStockpile() == 40);
    production.SetProduction(&itemB);
    CHECK(production.GetMineralStockpile() == 40);

    production.ApplyProduction(0, BaseEffects_t{});
    production.SetProduction(&itemA);
    CHECK(production.GetMineralStockpile() == 20);
}
