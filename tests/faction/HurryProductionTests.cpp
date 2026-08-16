#include "game/faction/base/production/HurryProductionCalculator.h"
#include "game/faction/base/production/ProductionConfigParser.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/IConstructable.h"
#include "game/faction/EconomyManager.h"
#include "lib/LuaRuntime.h"

#include "GameFixtures.h"
#include "TempConfigFile.h"

#include "game/Faction.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>

using namespace ac;
using namespace actest;

namespace
{

ProductionConfig_t SmacHurryConfig()
{
    ProductionConfig_t config;
    const auto kind = [](std::string formula, int mineralThreshold = 10,
                         int belowThresholdMultiplier = 2) {
        return HurryKindConfig_t{std::move(formula), mineralThreshold, belowThresholdMultiplier};
    };
    config.hurryKinds = {
        {ConstructableKind_t::Building, kind("2 * minerals")},
        {ConstructableKind_t::SecretProject, kind("4 * minerals")},
        {ConstructableKind_t::Unit, kind("floor(minerals * (2 + 0.05 * minerals))")},
    };
    return config;
}

HurryInputs_t Inputs(int remaining, int stockpile, ConstructableKind_t kind)
{
    return HurryInputs_t{remaining, stockpile, kind};
}

// Hurry the item in repeated instalments of `credits` until it is paid off, returning what
// the whole sequence cost.
int PayInInstalments(const HurryProductionCalculator& rCalculator, HurryInputs_t inputs,
                     int credits)
{
    int paid = 0;
    while (inputs.remainingMinerals > 0)
    {
        const HurrySpend_t spend = rCalculator.ApplyCredits(inputs, credits);
        if (spend.mineralsAdded <= 0)
        {
            break;
        }
        paid += spend.creditsSpent;
        inputs.remainingMinerals -= spend.mineralsAdded;
        inputs.mineralStockpile += spend.mineralsAdded;
    }
    return paid;
}

} // namespace

TEST_CASE("Below-threshold minerals are billed extra copies", "[production][hurry]")
{
    // multiplier 2: each mineral still needed to reach the threshold counts twice.
    CHECK(HurryProductionCalculator::BilledMinerals(20, 0, 10, 2) == 30);
    CHECK(HurryProductionCalculator::BilledMinerals(20, 10, 10, 2) == 20);
    CHECK(HurryProductionCalculator::BilledMinerals(20, 5, 10, 2) == 25);
    CHECK(HurryProductionCalculator::BilledMinerals(3, 8, 10, 2) == 5);
    CHECK(HurryProductionCalculator::BilledMinerals(0, 0, 10, 2) == 0);
    CHECK(HurryProductionCalculator::BilledMinerals(20, 0, 10, 1) == 20);
}

TEST_CASE("Hurry quote uses the formula for the item kind", "[production][hurry]")
{
    LuaRuntime lua;
    const ProductionConfig_t config = SmacHurryConfig();
    const HurryProductionCalculator calculator(config, lua);

    SECTION("building is 2 times billed minerals")
    {
        const HurryQuote_t quote = calculator.Quote(Inputs(20, 10, ConstructableKind_t::Building));
        CHECK(quote.bAvailable);
        CHECK(quote.remainingMinerals == 20);
        CHECK(quote.billedMinerals == 20);
        CHECK(quote.creditCost == 40);
    }

    SECTION("secret project is 4 times billed minerals")
    {
        const HurryQuote_t quote = calculator.Quote(Inputs(10, 10, ConstructableKind_t::SecretProject));
        CHECK(quote.billedMinerals == 10);
        CHECK(quote.creditCost == 40);
    }

    SECTION("unit is M*(2+0.05*M) on billed minerals")
    {
        const HurryQuote_t quote = calculator.Quote(Inputs(20, 10, ConstructableKind_t::Unit));
        CHECK(quote.billedMinerals == 20);
        CHECK(quote.creditCost == 60);
    }

    SECTION("a kind can use its own mineral_threshold and multiplier")
    {
        ProductionConfig_t custom = SmacHurryConfig();
        custom.hurryKinds.at(ConstructableKind_t::Building).mineralThreshold = 8;
        custom.hurryKinds.at(ConstructableKind_t::Building).belowThresholdMultiplier = 4;
        const HurryProductionCalculator customCalculator(custom, lua);
        // Building: 20 remaining from empty, 8 minerals billed 4×, 12 billed once → 44.
        const HurryQuote_t building = customCalculator.Quote(Inputs(20, 0, ConstructableKind_t::Building));
        CHECK(building.billedMinerals == 44);
        CHECK(building.creditCost == 88);
        // Unit still uses the shipped 10 / 2 band.
        const HurryQuote_t unit = customCalculator.Quote(Inputs(20, 0, ConstructableKind_t::Unit));
        CHECK(unit.billedMinerals == 30);
        CHECK(unit.creditCost == 105);
    }

    SECTION("minerals below mineral_threshold are billed extra before the formula")
    {
        const HurryQuote_t building = calculator.Quote(Inputs(20, 0, ConstructableKind_t::Building));
        CHECK(building.billedMinerals == 30);
        CHECK(building.creditCost == 60);

        const HurryQuote_t unit = calculator.Quote(Inputs(20, 0, ConstructableKind_t::Unit));
        CHECK(unit.billedMinerals == 30);
        CHECK(unit.creditCost == 105);
    }

    SECTION("nothing remaining costs nothing")
    {
        const HurryQuote_t quote = calculator.Quote(Inputs(0, 40, ConstructableKind_t::Building));
        CHECK(quote.bAvailable);
        CHECK(quote.creditCost == 0);
        CHECK(quote.billedMinerals == 0);
    }

    SECTION("a kind with no formula cannot be hurried")
    {
        const HurryQuote_t quote = calculator.Quote(Inputs(5, 0, ConstructableKind_t::Stockpile));
        CHECK_FALSE(quote.bAvailable);
        CHECK(quote.creditCost == 0);
        CHECK_THROWS_WITH(calculator.ApplyCredits(Inputs(5, 0, ConstructableKind_t::Stockpile), 100),
                          Catch::Matchers::ContainsSubstring("cannot be hurried"));
    }

    SECTION("dropping a kind from the config switches hurry off for it")
    {
        ProductionConfig_t noBuildings = SmacHurryConfig();
        noBuildings.hurryKinds.erase(ConstructableKind_t::Building);
        const HurryProductionCalculator restricted(noBuildings, lua);
        CHECK_FALSE(restricted.Quote(Inputs(20, 0, ConstructableKind_t::Building)).bAvailable);
        CHECK(restricted.Quote(Inputs(20, 0, ConstructableKind_t::Unit)).bAvailable);
    }
}

TEST_CASE("Hurry credits buy whole minerals at the quoted price", "[production][hurry]")
{
    LuaRuntime lua;
    const ProductionConfig_t config = SmacHurryConfig();
    const HurryProductionCalculator calculator(config, lua);
    // 20 minerals left with the stockpile already past the threshold: a flat 2 credits each.
    const HurryInputs_t inputs = Inputs(20, 10, ConstructableKind_t::Building);

    SECTION("full payment buys every remaining mineral")
    {
        const HurrySpend_t spend = calculator.ApplyCredits(inputs, 40);
        CHECK(spend.creditsSpent == 40);
        CHECK(spend.mineralsAdded == 20);
    }

    SECTION("half the credits buy half the minerals")
    {
        const HurrySpend_t spend = calculator.ApplyCredits(inputs, 20);
        CHECK(spend.creditsSpent == 20);
        CHECK(spend.mineralsAdded == 10);
    }

    SECTION("overpaying caps at the finish cost")
    {
        const HurrySpend_t spend = calculator.ApplyCredits(inputs, 1000);
        CHECK(spend.creditsSpent == 40);
        CHECK(spend.mineralsAdded == 20);
    }

    SECTION("credits that do not buy a whole mineral are not charged")
    {
        const HurrySpend_t spend = calculator.ApplyCredits(inputs, 1);
        CHECK(spend.creditsSpent == 0);
        CHECK(spend.mineralsAdded == 0);
    }

    SECTION("the charge is floored to whole minerals")
    {
        const HurrySpend_t spend = calculator.ApplyCredits(inputs, 11);
        CHECK(spend.mineralsAdded == 5);
        CHECK(spend.creditsSpent == 10);
    }

    SECTION("nothing left to buy spends nothing")
    {
        const HurrySpend_t spend = calculator.ApplyCredits(Inputs(0, 40, ConstructableKind_t::Building), 100);
        CHECK(spend.creditsSpent == 0);
        CHECK(spend.mineralsAdded == 0);
    }

    SECTION("a non-positive request is a caller error")
    {
        CHECK_THROWS_AS(calculator.ApplyCredits(inputs, 0), std::invalid_argument);
    }
}

TEST_CASE("Paying a hurry in instalments costs what paying it at once costs",
          "[production][hurry]")
{
    LuaRuntime lua;
    const ProductionConfig_t config = SmacHurryConfig();
    const HurryProductionCalculator calculator(config, lua);

    // Both price curves bend: the below-threshold band makes early minerals dearer, and the
    // unit formula grows faster than its input. Pricing an instalment as a flat fraction of
    // the finish cost let a player walk down either curve for less than the quoted price.
    SECTION("building bought from an empty stockpile")
    {
        const HurryInputs_t inputs = Inputs(20, 0, ConstructableKind_t::Building);
        const int oneShot = calculator.Quote(inputs).creditCost;
        CHECK(oneShot == 60);
        CHECK(PayInInstalments(calculator, inputs, oneShot / 2) == oneShot);
        CHECK(PayInInstalments(calculator, inputs, 7) == oneShot);
    }

    SECTION("unit bought a mineral or two at a time")
    {
        const HurryInputs_t inputs = Inputs(20, 10, ConstructableKind_t::Unit);
        const int oneShot = calculator.Quote(inputs).creditCost;
        CHECK(oneShot == 60);
        // 4 is what the dearest single mineral costs; anything less buys nothing at all.
        CHECK(PayInInstalments(calculator, inputs, 4) == oneShot);
        CHECK(PayInInstalments(calculator, inputs, 9) == oneShot);
    }
}

TEST_CASE("Production config requires hurry kinds", "[production][hurry][config]")
{
    SECTION("shipping shape loads")
    {
        const TempConfigFile file("hurry_ok.json", R"({
            "retool_penalty_threshold": 10,
            "retool_penalty_percent": 50,
            "prototype_surcharge_percent": 50,
            "hurry": {
                "building": {
                    "formula": "2 * minerals",
                    "mineral_threshold": 8,
                    "below_threshold_multiplier": 3
                }
            }
        })");
        const ProductionConfig_t config = ProductionConfigParser{}.ParseConfig(file.Path());
        REQUIRE(config.hurryKinds.count(ConstructableKind_t::Building) == 1);
        CHECK(config.hurryKinds.at(ConstructableKind_t::Building).formula == "2 * minerals");
        CHECK(config.hurryKinds.at(ConstructableKind_t::Building).mineralThreshold == 8);
        CHECK(config.hurryKinds.at(ConstructableKind_t::Building).belowThresholdMultiplier == 3);
    }

    SECTION("kinds do not share mineral_threshold or multiplier")
    {
        const TempConfigFile file("hurry_per_kind.json", R"({
            "retool_penalty_threshold": 40,
            "retool_penalty_percent": 50,
            "prototype_surcharge_percent": 50,
            "hurry": {
                "building": {
                    "formula": "2 * minerals",
                    "mineral_threshold": 10,
                    "below_threshold_multiplier": 2
                },
                "unit": {
                    "formula": "3 * minerals",
                    "mineral_threshold": 0,
                    "below_threshold_multiplier": 1
                }
            }
        })");
        const ProductionConfig_t config = ProductionConfigParser{}.ParseConfig(file.Path());
        CHECK(config.retoolPenaltyThreshold == 40);
        CHECK(config.hurryKinds.at(ConstructableKind_t::Building).mineralThreshold == 10);
        CHECK(config.hurryKinds.at(ConstructableKind_t::Building).belowThresholdMultiplier == 2);
        CHECK(config.hurryKinds.at(ConstructableKind_t::Unit).mineralThreshold == 0);
        CHECK(config.hurryKinds.at(ConstructableKind_t::Unit).belowThresholdMultiplier == 1);
    }

    SECTION("an empty hurry object disables hurrying rather than failing to load")
    {
        const TempConfigFile file("hurry_empty.json", R"({
            "retool_penalty_threshold": 10,
            "retool_penalty_percent": 50,
            "prototype_surcharge_percent": 50,
            "hurry": {}
        })");
        const ProductionConfig_t config = ProductionConfigParser{}.ParseConfig(file.Path());
        CHECK(config.hurryKinds.empty());
    }

    SECTION("missing hurry object throws")
    {
        const TempConfigFile file("hurry_missing.json", R"({
            "retool_penalty_threshold": 10,
            "retool_penalty_percent": 50,
            "prototype_surcharge_percent": 50
        })");
        CHECK_THROWS_WITH(ProductionConfigParser{}.ParseConfig(file.Path()),
                          Catch::Matchers::ContainsSubstring("hurry"));
    }

    SECTION("a kind missing mineral_threshold throws")
    {
        const TempConfigFile file("hurry_no_threshold.json", R"({
            "retool_penalty_threshold": 10,
            "retool_penalty_percent": 50,
            "prototype_surcharge_percent": 50,
            "hurry": { "building": { "formula": "1", "below_threshold_multiplier": 2 } }
        })");
        CHECK_THROWS_WITH(ProductionConfigParser{}.ParseConfig(file.Path()),
                          Catch::Matchers::ContainsSubstring("hurry.building")
                              && Catch::Matchers::ContainsSubstring("mineral_threshold"));
    }

    SECTION("a kind missing below_threshold_multiplier throws")
    {
        const TempConfigFile file("hurry_no_multiplier.json", R"({
            "retool_penalty_threshold": 10,
            "retool_penalty_percent": 50,
            "prototype_surcharge_percent": 50,
            "hurry": { "building": { "formula": "1", "mineral_threshold": 10 } }
        })");
        CHECK_THROWS_WITH(ProductionConfigParser{}.ParseConfig(file.Path()),
                          Catch::Matchers::ContainsSubstring("below_threshold_multiplier"));
    }

    SECTION("a kind missing formula throws")
    {
        const TempConfigFile file("hurry_no_formula.json", R"({
            "retool_penalty_threshold": 10,
            "retool_penalty_percent": 50,
            "prototype_surcharge_percent": 50,
            "hurry": { "building": { "mineral_threshold": 10, "below_threshold_multiplier": 2 } }
        })");
        CHECK_THROWS_WITH(ProductionConfigParser{}.ParseConfig(file.Path()),
                          Catch::Matchers::ContainsSubstring("formula"));
    }

    SECTION("multiplier below 1 throws")
    {
        const TempConfigFile file("hurry_mult.json", R"({
            "retool_penalty_threshold": 10,
            "retool_penalty_percent": 50,
            "prototype_surcharge_percent": 50,
            "hurry": { "building": { "formula": "1", "mineral_threshold": 10,
                                     "below_threshold_multiplier": 0 } }
        })");
        CHECK_THROWS_WITH(ProductionConfigParser{}.ParseConfig(file.Path()),
                          Catch::Matchers::ContainsSubstring("below_threshold_multiplier"));
    }

    SECTION("an unknown kind throws")
    {
        const TempConfigFile file("hurry_unknown.json", R"({
            "retool_penalty_threshold": 10,
            "retool_penalty_percent": 50,
            "prototype_surcharge_percent": 50,
            "hurry": { "wonder": { "formula": "1", "mineral_threshold": 10,
                                   "below_threshold_multiplier": 2 } }
        })");
        CHECK_THROWS_WITH(ProductionConfigParser{}.ParseConfig(file.Path()),
                          Catch::Matchers::ContainsSubstring("hurry.wonder")
                              && Catch::Matchers::ContainsSubstring("known constructable kind"));
    }

    SECTION("stockpile cannot be a hurry kind")
    {
        const TempConfigFile file("hurry_stockpile.json", R"({
            "retool_penalty_threshold": 10,
            "retool_penalty_percent": 50,
            "prototype_surcharge_percent": 50,
            "hurry": { "stockpile": { "formula": "1", "mineral_threshold": 10,
                                      "below_threshold_multiplier": 2 } }
        })");
        CHECK_THROWS_WITH(ProductionConfigParser{}.ParseConfig(file.Path()),
                          Catch::Matchers::ContainsSubstring("hurry.stockpile")
                              && Catch::Matchers::ContainsSubstring("cannot be hurried"));
    }
}

TEST_CASE("Hurry spends treasury credits into the production stockpile",
          "[production][hurry][base]")
{
    // Completing a hurried item dispatches Instantaneous effects, which need a bound GameState.
    FactionFixture fixtures;
    GameSettings settings;
    auto pMap = std::make_unique<WorldMap>(9, 9);
    for (auto& pTile : pMap->GetTiles())
    {
        pTile->SetElevation(100);
    }
    auto pState = std::make_unique<GameState>(
        std::move(pMap), fixtures.improvements, &fixtures.unitComponents, settings,
        *fixtures.dataContext.moraleCalculator, k_TestRngSeed);
    Faction& rFaction = pState->AddFaction(std::make_unique<Faction>(
        pState->AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext,
        pState->GetWorldMap(), fixtures.settings, k_TestFactionSeed));
    BaseManager* pBase = rFaction.CreateBase(
        pState->AllocateBaseId(), "TestBase", pState->GetWorldMap().GetTile(4, 4),
        fixtures.dataContext, pState->GetTileEffects(),
        pState->GetSecretProjectAvailability());
    REQUIRE(pBase != nullptr);

    // A costed facility: the fixtures' default 0-mineral buildings finish in one mineral, and
    // a single mineral can only ever be bought outright.
    const BuildingConfig_t* pFacility = fixtures.buildings().Find("test_hurry_facility");
    REQUIRE(pFacility != nullptr);
    REQUIRE(pFacility->GetConstructableKind() == ConstructableKind_t::Building);

    pBase->GetProduction().SetProduction(pFacility);
    REQUIRE(pBase->GetMineralCost() >= 1);
    pBase->GetProduction().SetMineralStockpile(0);

    const HurryQuote_t quote = pBase->QuoteHurry();
    REQUIRE(quote.bAvailable);
    REQUIRE(quote.remainingMinerals == pBase->GetMineralCost());
    REQUIRE(quote.creditCost > 0);

    SECTION("full hurry completes the facility")
    {
        rFaction.GetEconomy().AddEnergy(quote.creditCost);
        const HurryResult_t hurried = pBase->HurryProduction(quote.creditCost);
        CHECK(hurried.creditsSpent == quote.creditCost);
        CHECK(hurried.mineralsAdded == quote.remainingMinerals);
        CHECK(rFaction.GetEconomy().GetEnergy() == 0);
        CHECK(hurried.production.kind == ProductionApplyKind_t::Completed);
        CHECK(hurried.production.completedId == pFacility->id);
    }

    SECTION("a partial hurry banks minerals and leaves the item queued")
    {
        rFaction.GetEconomy().AddEnergy(quote.creditCost);
        const HurryResult_t hurried = pBase->HurryProduction(quote.creditCost / 2);
        CHECK(hurried.mineralsAdded > 0);
        CHECK(hurried.mineralsAdded < quote.remainingMinerals);
        CHECK(pBase->GetProduction().GetMineralStockpile() == hurried.mineralsAdded);
        CHECK(rFaction.GetEconomy().GetEnergy() == quote.creditCost - hurried.creditsSpent);
        CHECK(hurried.production.kind == ProductionApplyKind_t::InProgress);

        // Finishing from there costs the rest of the original quote, not a fresh discount.
        const HurryQuote_t rest = pBase->QuoteHurry();
        CHECK(hurried.creditsSpent + rest.creditCost == quote.creditCost);
    }

    SECTION("an overdraft is refused before anything is granted")
    {
        rFaction.GetEconomy().AddEnergy(quote.creditCost - 1);
        CHECK_THROWS(pBase->HurryProduction(quote.creditCost));
        CHECK(pBase->GetProduction().GetMineralStockpile() == 0);
        CHECK(rFaction.GetEconomy().GetEnergy() == quote.creditCost - 1);
    }

    SECTION("a stockpile cannot be hurried")
    {
        pBase->GetProduction().SetProduction(nullptr);
        REQUIRE(pBase->GetProduction().GetCurrentProduction() != nullptr);
        REQUIRE(pBase->GetProduction().GetCurrentProduction()->NeverCompletes());
        CHECK_FALSE(pBase->QuoteHurry().bAvailable);
        rFaction.GetEconomy().AddEnergy(10);
        CHECK_THROWS_WITH(pBase->HurryProduction(10),
                          Catch::Matchers::ContainsSubstring("cannot be hurried"));
        CHECK(rFaction.GetEconomy().GetEnergy() == 10);
    }

    SECTION("secret projects use the secret_project formula")
    {
        const BuildingConfig_t* pProject = fixtures.buildings().Find("test_secret_project");
        REQUIRE(pProject != nullptr);
        REQUIRE(pProject->GetConstructableKind() == ConstructableKind_t::SecretProject);
        pBase->GetProduction().SetProduction(pProject);
        pBase->GetProduction().SetMineralStockpile(0);
        const HurryQuote_t projectQuote = pBase->QuoteHurry();
        CHECK(projectQuote.creditCost == 4 * projectQuote.billedMinerals);
    }
}
