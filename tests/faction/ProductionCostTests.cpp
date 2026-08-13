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

    SECTION("no CostMultiplier effects is normal rate (base_cost)")
    {
        CHECK(ProductionCostCalculator::ComputeCost(10, BaseEffects_t{}) == 10);
    }

    SECTION("negative CostMultiplier AddPercent reduces cost (Industry +)")
    {
        CHECK(ProductionCostCalculator::ComputeCost(10, WithCostPercent(pool, -10.0)) == 9);
        CHECK(ProductionCostCalculator::ComputeCost(10, WithCostPercent(pool, -50.0)) == 5);
    }

    SECTION("positive CostMultiplier AddPercent raises cost (Industry -)")
    {
        CHECK(ProductionCostCalculator::ComputeCost(10, WithCostPercent(pool, 10.0)) == 11);
        CHECK(ProductionCostCalculator::ComputeCost(10, WithCostPercent(pool, 30.0)) == 13);
    }

    SECTION("CostMultiplier that zeroes the cost still floors at 1")
    {
        CHECK(ProductionCostCalculator::ComputeCost(10, WithCostPercent(pool, -100.0)) == 1);
    }

    SECTION("prototype surcharge is 50% more minerals and does not stack in the calculator")
    {
        CHECK(ProductionCostCalculator::ComputeCost(10, BaseEffects_t{}, 50) == 15);
        CHECK(ProductionCostCalculator::ComputeCost(10, WithCostPercent(pool, -20.0), 50) == 12);
    }
}

TEST_CASE("ProductionManager resolves cost from base effects", "[production][cost]")
{
    ProductionManager production(k_TestConfig);
    StubConstructable item;
    production.SetProduction(&item);

    actest::EffectPool pool;
    CHECK(production.GetMineralCost(BaseEffects_t{}) == 10);
    CHECK(production.GetMineralCost(WithCostPercent(pool, -20.0)) == 8);

    production.BankProduction(5);
    CHECK_FALSE(production.IsReadyToComplete(BaseEffects_t{}));
    CHECK(production.GetMineralStockpile() == 5);
    CHECK(production.HasProduction());

    production.BankProduction(5);
    REQUIRE(production.IsReadyToComplete(BaseEffects_t{}));
    CHECK(production.CompleteProduction() == "stub_item");
    CHECK_FALSE(production.HasProduction());
}

// Retooling. Switching away from what the base started the turn on forfeits half the minerals
// already spent, once more than the threshold has accumulated; switching back is free; switching
// on to a third item pays again. See docs/game-rules-decisions.md.
//
// Items cost well above the stockpiles these cases bank, so BankProduction(0) only stamps the
// turn original and never completes mid-scenario.
namespace
{
StubConstructable RetoolItem(std::string id, std::string name)
{
    StubConstructable item{std::move(id), std::move(name)};
    item.baseCost = 100;
    return item;
}
} // namespace

TEST_CASE("Retooling forfeits half the minerals spent past the threshold", "[production][retool]")
{
    const StubConstructable itemA = RetoolItem("a", "A");
    const StubConstructable itemB = RetoolItem("b", "B");

    ProductionManager production(k_TestConfig);
    production.SetProduction(&itemA);
    production.BankProduction(0); // marks A as this turn's original
    production.SetMineralStockpile(40);

    production.SetProduction(&itemB);
    CHECK(production.GetMineralStockpile() == 20);
}

TEST_CASE("Retooling is free at or below the threshold", "[production][retool]")
{
    const StubConstructable itemA = RetoolItem("a", "A");
    const StubConstructable itemB = RetoolItem("b", "B");

    ProductionManager production(k_TestConfig);
    production.SetProduction(&itemA);
    production.BankProduction(0);
    production.SetMineralStockpile(k_TestConfig.retoolPenaltyThreshold);

    production.SetProduction(&itemB);
    CHECK(production.GetMineralStockpile() == k_TestConfig.retoolPenaltyThreshold);
}

TEST_CASE("Switching back to the turn's original item is free", "[production][retool]")
{
    const StubConstructable itemA = RetoolItem("a", "A");
    const StubConstructable itemB = RetoolItem("b", "B");

    ProductionManager production(k_TestConfig);
    production.SetProduction(&itemA);
    production.BankProduction(0);
    production.SetMineralStockpile(40);

    production.SetProduction(&itemB);
    REQUIRE(production.GetMineralStockpile() == 20);

    // Back to A: no second charge, and no refund either.
    production.SetProduction(&itemA);
    CHECK(production.GetMineralStockpile() == 20);
}

TEST_CASE("Switching on to a third item pays again", "[production][retool]")
{
    const StubConstructable itemA = RetoolItem("a", "A");
    const StubConstructable itemB = RetoolItem("b", "B");
    const StubConstructable itemC = RetoolItem("c", "C");

    ProductionManager production(k_TestConfig);
    production.SetProduction(&itemA);
    production.BankProduction(0);
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
    const StubConstructable itemA = RetoolItem("a", "A");
    const StubConstructable itemB = RetoolItem("b", "B");

    ProductionManager production(k_TestConfig);
    production.SetProduction(&itemA);
    production.BankProduction(0);
    production.SetMineralStockpile(40);
    production.SetProduction(&itemB);
    REQUIRE(production.GetMineralStockpile() == 20);

    // Next turn: B is now the original.
    production.BankProduction(0);
    production.SetMineralStockpile(40);
    production.SetProduction(&itemA);
    CHECK(production.GetMineralStockpile() == 20);
}

TEST_CASE("Null turn original skips retool until BankProduction stamps one",
          "[production][retool]")
{
    // Fresh manager (and founding mineral banks) have no turn original — queue/switch free
    // until BankProduction banks with something queued.
    const StubConstructable itemA = RetoolItem("a", "A");
    const StubConstructable itemB = RetoolItem("b", "B");

    ProductionManager production(k_TestConfig);
    production.SetMineralStockpile(40);

    production.SetProduction(&itemA);
    CHECK(production.GetMineralStockpile() == 40);
    production.SetProduction(&itemB);
    CHECK(production.GetMineralStockpile() == 40);

    production.BankProduction(0);
    production.SetProduction(&itemA);
    CHECK(production.GetMineralStockpile() == 20);
}
