#include "game/faction/base/production/ProductionCostCalculator.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/IConstructable.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"

#include "GameFixtures.h"
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
    ConstructableKind_t GetConstructableKind() const override
    {
        return ConstructableKind_t::Building;
    }
};

BaseEffects_t WithCostPercent(const BaseManager& rBase, actest::EffectPool& rPool, double addPercent)
{
    return BaseEffects_t{rBase, {
        Active(rPool.StatMod(StatId_t::CostMultiplier, addPercent, ModifierOp_t::AddPercent), "industry"),
    }};
}

BaseEffects_t WithSurchargeScale(const BaseManager& rBase, actest::EffectPool& rPool,
                                 double geometricFactor)
{
    return BaseEffects_t{rBase, {
        Active(rPool.StatMod(StatId_t::PrototypeSurchargeScale, geometricFactor,
                             ModifierOp_t::MultiplyGeometric),
               "skunkworks"),
    }};
}

BaseEffects_t WithCostAndSurchargeScale(const BaseManager& rBase, actest::EffectPool& rPool,
                                        double costAddPercent, double surchargeGeometric)
{
    return BaseEffects_t{rBase, {
        Active(rPool.StatMod(StatId_t::CostMultiplier, costAddPercent, ModifierOp_t::AddPercent),
               "industry"),
        Active(rPool.StatMod(StatId_t::PrototypeSurchargeScale, surchargeGeometric,
                             ModifierOp_t::MultiplyGeometric),
               "skunkworks"),
    }};
}

} // namespace

TEST_CASE("Production cost applies CostMultiplier effects like GrowthRate", "[production][cost]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);
    actest::EffectPool pool;

    SECTION("no CostMultiplier effects is normal rate (base_cost)")
    {
        CHECK(ProductionCostCalculator::ComputeCost(10, BaseEffects_t{base}, 0) == 10);
    }

    SECTION("negative CostMultiplier AddPercent reduces cost (Industry +)")
    {
        CHECK(ProductionCostCalculator::ComputeCost(10, WithCostPercent(base, pool, -10.0), 0) == 9);
        CHECK(ProductionCostCalculator::ComputeCost(10, WithCostPercent(base, pool, -50.0), 0) == 5);
    }

    SECTION("positive CostMultiplier AddPercent raises cost (Industry -)")
    {
        CHECK(ProductionCostCalculator::ComputeCost(10, WithCostPercent(base, pool, 10.0), 0) == 11);
        CHECK(ProductionCostCalculator::ComputeCost(10, WithCostPercent(base, pool, 30.0), 0) == 13);
    }

    SECTION("CostMultiplier that zeroes the cost still floors at 1")
    {
        CHECK(ProductionCostCalculator::ComputeCost(10, WithCostPercent(base, pool, -100.0), 0) == 1);
    }

    SECTION("prototype surcharge is 50% more minerals and does not stack in the calculator")
    {
        CHECK(ProductionCostCalculator::ComputeCost(10, BaseEffects_t{base}, 50) == 15);
        CHECK(ProductionCostCalculator::ComputeCost(10, WithCostPercent(base, pool, -20.0), 50) == 12);
    }

    SECTION("PrototypeSurchargeScale 0 cancels only the surcharge term")
    {
        CHECK(ProductionCostCalculator::ComputeCost(10, WithSurchargeScale(base, pool, 0.0), 50) == 10);
        CHECK(ProductionCostCalculator::ComputeCost(
                  10, WithCostAndSurchargeScale(base, pool, -20.0, 0.0), 50)
              == 8);
    }

    SECTION("PrototypeSurchargeScale default seed leaves the full surcharge")
    {
        CHECK(ProductionCostCalculator::ComputeCost(10, WithSurchargeScale(base, pool, 1.0), 50) == 15);
    }
}

TEST_CASE("ProductionManager resolves cost from base effects", "[production][cost]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);
    ProductionManager production(k_TestConfig, nullptr);
    StubConstructable item;
    production.SetProduction(&item, BaseEffects_t{base});

    actest::EffectPool pool;
    CHECK(production.GetMineralCost(BaseEffects_t{base}, false) == 10);
    CHECK(production.GetMineralCost(WithCostPercent(base, pool, -20.0), false) == 8);

    production.BankProduction(5);
    CHECK_FALSE(production.IsReadyToComplete(BaseEffects_t{base}, false));
    CHECK(production.GetMineralStockpile() == 5);
    CHECK(production.HasProduction());

    production.BankProduction(5);
    REQUIRE(production.IsReadyToComplete(BaseEffects_t{base}, false));
    CHECK(production.CompleteProduction(BaseEffects_t{base}) == "stub_item");
    CHECK_FALSE(production.HasProduction());
    CHECK(production.GetMineralStockpile() == 0);
}

TEST_CASE("A never-completing item has no mineral cost and is never ready",
          "[production][stockpile]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);
    struct NeverCompleteItem : StubConstructable
    {
        bool NeverCompletes() const override { return true; }
    };

    NeverCompleteItem item;
    item.baseCost = 0;
    ProductionManager production(k_TestConfig, nullptr);
    production.SetProduction(&item, BaseEffects_t{base});
    production.SetMineralStockpile(100);

    CHECK(production.GetMineralCost(BaseEffects_t{base}, false) == 0);
    CHECK_FALSE(production.IsReadyToComplete(BaseEffects_t{base}, false));

    production.BankProduction(10);
    CHECK(production.GetMineralStockpile() == 110);
    CHECK_FALSE(production.IsReadyToComplete(BaseEffects_t{base}, false));
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
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);
    const StubConstructable itemA = RetoolItem("a", "A");
    const StubConstructable itemB = RetoolItem("b", "B");

    ProductionManager production(k_TestConfig, nullptr);
    production.SetProduction(&itemA, BaseEffects_t{base});
    production.BankProduction(0); // marks A as this turn's original
    production.SetMineralStockpile(40);

    production.SetProduction(&itemB, BaseEffects_t{base});
    CHECK(production.GetMineralStockpile() == 20);
}

TEST_CASE("RetoolPenaltyScale 0 cancels the forfeit", "[production][retool]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);
    const StubConstructable itemA = RetoolItem("a", "A");
    const StubConstructable itemB = RetoolItem("b", "B");
    actest::EffectPool pool;
    const BaseEffects_t noRetool{base, {
        Active(pool.StatMod(StatId_t::RetoolPenaltyScale, 0.0, ModifierOp_t::MultiplyGeometric),
               "skunkworks"),
    }};

    ProductionManager production(k_TestConfig, nullptr);
    production.SetProduction(&itemA, BaseEffects_t{base});
    production.BankProduction(0);
    production.SetMineralStockpile(40);

    production.SetProduction(&itemB, noRetool);
    CHECK(production.GetMineralStockpile() == 40);
}

TEST_CASE("Retooling is free at or below the threshold", "[production][retool]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);
    const StubConstructable itemA = RetoolItem("a", "A");
    const StubConstructable itemB = RetoolItem("b", "B");

    ProductionManager production(k_TestConfig, nullptr);
    production.SetProduction(&itemA, BaseEffects_t{base});
    production.BankProduction(0);
    production.SetMineralStockpile(k_TestConfig.retoolPenaltyThreshold);

    production.SetProduction(&itemB, BaseEffects_t{base});
    CHECK(production.GetMineralStockpile() == k_TestConfig.retoolPenaltyThreshold);
}

TEST_CASE("Switching back to the turn's original item is free", "[production][retool]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);
    const StubConstructable itemA = RetoolItem("a", "A");
    const StubConstructable itemB = RetoolItem("b", "B");

    ProductionManager production(k_TestConfig, nullptr);
    production.SetProduction(&itemA, BaseEffects_t{base});
    production.BankProduction(0);
    production.SetMineralStockpile(40);

    production.SetProduction(&itemB, BaseEffects_t{base});
    REQUIRE(production.GetMineralStockpile() == 20);

    // Back to A: no second charge, and no refund either.
    production.SetProduction(&itemA, BaseEffects_t{base});
    CHECK(production.GetMineralStockpile() == 20);
}

TEST_CASE("Switching on to a third item pays again", "[production][retool]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);
    const StubConstructable itemA = RetoolItem("a", "A");
    const StubConstructable itemB = RetoolItem("b", "B");
    const StubConstructable itemC = RetoolItem("c", "C");

    ProductionManager production(k_TestConfig, nullptr);
    production.SetProduction(&itemA, BaseEffects_t{base});
    production.BankProduction(0);
    production.SetMineralStockpile(40);

    production.SetProduction(&itemB, BaseEffects_t{base});
    REQUIRE(production.GetMineralStockpile() == 20);
    production.SetProduction(&itemC, BaseEffects_t{base});
    CHECK(production.GetMineralStockpile() == 10);
}

TEST_CASE("The turn's original item follows production, turn by turn", "[production][retool]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);
    // A switch made last turn becomes this turn's baseline: going back to what you built the
    // turn before is a retool like any other.
    const StubConstructable itemA = RetoolItem("a", "A");
    const StubConstructable itemB = RetoolItem("b", "B");

    ProductionManager production(k_TestConfig, nullptr);
    production.SetProduction(&itemA, BaseEffects_t{base});
    production.BankProduction(0);
    production.SetMineralStockpile(40);
    production.SetProduction(&itemB, BaseEffects_t{base});
    REQUIRE(production.GetMineralStockpile() == 20);

    // Next turn: B is now the original.
    production.BankProduction(0);
    production.SetMineralStockpile(40);
    production.SetProduction(&itemA, BaseEffects_t{base});
    CHECK(production.GetMineralStockpile() == 20);
}

TEST_CASE("Null turn original skips retool until BankProduction stamps one",
          "[production][retool]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);
    // Fresh manager (and founding mineral banks) have no turn original — queue/switch free
    // until BankProduction banks with something queued.
    const StubConstructable itemA = RetoolItem("a", "A");
    const StubConstructable itemB = RetoolItem("b", "B");

    ProductionManager production(k_TestConfig, nullptr);
    production.SetMineralStockpile(40);

    production.SetProduction(&itemA, BaseEffects_t{base});
    CHECK(production.GetMineralStockpile() == 40);
    production.SetProduction(&itemB, BaseEffects_t{base});
    CHECK(production.GetMineralStockpile() == 40);

    production.BankProduction(0);
    production.SetProduction(&itemA, BaseEffects_t{base});
    CHECK(production.GetMineralStockpile() == 20);
}

// Completion leftover. Minerals past the item's effective cost stay on the next queued item
// (the default fallback, or whatever the player queues next), but only up to the retool
// threshold — so choosing the next build is a free switch. See CompleteProduction.
TEST_CASE("Completion leftover minerals carry to the next item up to the retool threshold",
          "[production][retool]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);
    StubConstructable item;
    item.baseCost = 10;
    StubConstructable nextDefault{"next", "Next"};
    nextDefault.baseCost = 100;

    ProductionManager production(k_TestConfig, [&] { return &nextDefault; });
    production.SetProduction(&item, BaseEffects_t{base});

    SECTION("exact cost leaves nothing")
    {
        production.SetMineralStockpile(10);
        CHECK(production.CompleteProduction(BaseEffects_t{base}) == "stub_item");
        CHECK(production.GetCurrentProduction() == &nextDefault);
        CHECK(production.GetMineralStockpile() == 0);
    }

    SECTION("leftover below the threshold is kept in full")
    {
        production.SetMineralStockpile(17);
        CHECK(production.CompleteProduction(BaseEffects_t{base}) == "stub_item");
        CHECK(production.GetCurrentProduction() == &nextDefault);
        CHECK(production.GetMineralStockpile() == 7);
    }

    SECTION("leftover above the threshold is capped")
    {
        production.SetMineralStockpile(40);
        CHECK(production.CompleteProduction(BaseEffects_t{base}) == "stub_item");
        CHECK(production.GetCurrentProduction() == &nextDefault);
        CHECK(production.GetMineralStockpile() == k_TestConfig.retoolPenaltyThreshold);
    }
}

TEST_CASE("Completion leftover uses the effective cost including prototype surcharge",
          "[production][retool][prototype]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);
    StubConstructable item;
    item.baseCost = 10; // 50% surcharge → 15
    ProductionManager production(k_TestConfig, nullptr);
    production.SetProduction(&item, BaseEffects_t{base});
    production.SetMineralStockpile(20);

    CHECK(production.CompleteProduction(BaseEffects_t{base}, true) == "stub_item");
    CHECK(production.GetMineralStockpile() == 5);
}

TEST_CASE("Completion leftover uses CostMultiplier when computing what was spent",
          "[production][retool][cost]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);
    actest::EffectPool pool;
    StubConstructable item;
    item.baseCost = 10; // -20% Industry → 8
    ProductionManager production(k_TestConfig, nullptr);
    production.SetProduction(&item, BaseEffects_t{base});
    production.SetMineralStockpile(12);

    CHECK(production.CompleteProduction(WithCostPercent(base, pool, -20.0), false) == "stub_item");
    CHECK(production.GetMineralStockpile() == 4);
}

TEST_CASE("Completion leftover on the next item is a free retool", "[production][retool]")
{
    actest::BaseFixture fixture;
    BaseManager& base = fixture.MakeBase(4, 4);
    StubConstructable item;
    item.baseCost = 10;
    StubConstructable nextDefault{"stockpile", "Stockpile"};
    const StubConstructable playerPick = RetoolItem("pick", "Pick");

    ProductionManager production(k_TestConfig, [&] { return &nextDefault; });
    production.SetProduction(&item, BaseEffects_t{base});
    production.BankProduction(0);
    production.SetMineralStockpile(40);

    REQUIRE(production.CompleteProduction(BaseEffects_t{base}) == "stub_item");
    REQUIRE(production.GetCurrentProduction() == &nextDefault);
    REQUIRE(production.GetMineralStockpile() == k_TestConfig.retoolPenaltyThreshold);

    // Next turn's BankProduction would stamp the fallback as original; leftover is still
    // at the threshold, so picking a real build does not forfeit.
    production.BankProduction(0);
    production.SetProduction(&playerPick, BaseEffects_t{base});
    CHECK(production.GetCurrentProduction() == &playerPick);
    CHECK(production.GetMineralStockpile() == k_TestConfig.retoolPenaltyThreshold);
}
