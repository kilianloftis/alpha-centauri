// Player scrap of a constructed building: production.json kinds.building.default_scrap
// formula over listed mineral cost, paid as energy credits. Combat / probe / raze
// destruction does not refund.

#include "game/faction/base/production/ScrapRefundCalculator.h"
#include "game/faction/base/production/ProductionConfigParser.h"
#include "game/faction/base/production/ScrapConfigParser.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "lib/LuaRuntime.h"

#include "GameFixtures.h"
#include "TempConfigFile.h"

#include "game/buildings/BuildingConfig.h"
#include "game/ConstructableKind.h"
#include "game/Faction.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/buildings/SecretProjectAvailabilityCalculator.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/map/WorldMap.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <nlohmann/json.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ac;
using namespace actest;

namespace
{

ProductionConfig_t ScrapConfig(std::string formula, int ceilingPercent = 100)
{
    ProductionConfig_t config;
    ScrapKindConfig_t scrap;
    scrap.formula = std::move(formula);
    scrap.refundType = StatId_t::EnergyCredits;
    scrap.refundCeilingPercent = ceilingPercent;
    config.kinds[ConstructableKind_t::Building].defaultScrap = std::move(scrap);
    return config;
}

} // namespace

TEST_CASE("Scrap refund evaluates the kind's formula over listed mineral cost", "[building][scrap]")
{
    LuaRuntime lua;
    const ProductionConfig_t halfConfig = ScrapConfig("floor(minerals / 2)");
    const ScrapRefundCalculator half(halfConfig, lua);
    CHECK(half.Quote(0, ConstructableKind_t::Building).amount == 0);
    CHECK(half.Quote(1, ConstructableKind_t::Building).amount == 0);
    CHECK(half.Quote(5, ConstructableKind_t::Building).amount == 2);
    const ScrapQuote_t halfQuote = half.Quote(20, ConstructableKind_t::Building);
    CHECK(halfQuote.amount == 10);
    CHECK(halfQuote.refundType == StatId_t::EnergyCredits);

    const ProductionConfig_t fullConfig = ScrapConfig("minerals");
    const ScrapRefundCalculator full(fullConfig, lua);
    CHECK(full.Quote(20, ConstructableKind_t::Building).amount == 20);
    CHECK(full.Quote(21, ConstructableKind_t::Building).amount == 21);
}

TEST_CASE("Scrap refund applies config override, bonuses, then the ceiling", "[building][scrap]")
{
    LuaRuntime lua;
    ProductionConfig_t config = ScrapConfig("floor(minerals / 2)", 100);
    config.kinds[ConstructableKind_t::Building].defaultScrap->refundType =
        StatId_t::EnergyCredits;
    const ScrapRefundCalculator calculator(config, lua);

    ScrapOverride_t fullCost;
    fullCost.formula = "minerals";
    const ScrapQuote_t overridden = calculator.Quote(21, ConstructableKind_t::Building, fullCost);
    CHECK(overridden.amount == 21);
    CHECK(overridden.refundType == StatId_t::EnergyCredits);

    ScrapOverride_t mineralsOnly;
    mineralsOnly.refundType = StatId_t::Minerals;
    CHECK(calculator.Quote(20, ConstructableKind_t::Building, mineralsOnly).refundType
          == StatId_t::Minerals);
    CHECK(calculator.Quote(20, ConstructableKind_t::Building, mineralsOnly).amount == 10);

    EffectConfig_t bonusConfig;
    bonusConfig.scope = EffectScope_t::ThisBase;
    StatModifierEffect_t bonusMod;
    bonusMod.stat = StatId_t::ScrapRefund;
    bonusMod.amount = 100;
    bonusMod.op = ModifierOp_t::AddPercent;
    bonusConfig.effect = bonusMod;
    const ActiveEffect_t bonus(bonusConfig, "scrap_bonus");
    const std::vector<ActiveEffect_t> bonuses = {bonus};
    CHECK(calculator.Quote(20, ConstructableKind_t::Building, {}, bonuses).amount == 20);

    const ProductionConfig_t capped = ScrapConfig("floor(minerals / 2)", 50);
    const ScrapRefundCalculator cappedCalc(capped, lua);
    CHECK(cappedCalc.Quote(20, ConstructableKind_t::Building, {}, bonuses).amount == 10);
}

TEST_CASE("A constructable kind with no scrap entry cannot be scrapped", "[building][scrap]")
{
    LuaRuntime lua;
    const ProductionConfig_t config;
    const ScrapRefundCalculator calculator(config, lua);
    CHECK_FALSE(calculator.Quote(20, ConstructableKind_t::Building).bAvailable);
}

TEST_CASE("A null formula override denies scrap even when the kind has a default",
          "[building][scrap]")
{
    LuaRuntime lua;
    const ProductionConfig_t config = ScrapConfig("floor(minerals / 2)");
    const ScrapRefundCalculator calculator(config, lua);
    REQUIRE(calculator.Quote(20, ConstructableKind_t::Building).bAvailable);

    ScrapOverride_t deny;
    deny.formula = "";
    CHECK(ScrapOverrideDenies(deny));
    CHECK_FALSE(calculator.Quote(20, ConstructableKind_t::Building, deny).bAvailable);
}

TEST_CASE("Scrapping a building grants half its mineral cost as energy and removes it",
          "[building][scrap]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 2, 2);
    REQUIRE(faction.GetEconomy().GetEnergy() == 0);

    base.GetBuildingManager().AddBuilding("test_hurry_facility");
    REQUIRE(base.GetBuildingManager().HasBuilding("test_hurry_facility"));
    REQUIRE(faction.QuoteScrapBuilding(base, "test_hurry_facility")->amount == 10);

    CHECK(faction.ScrapBuilding(base, "test_hurry_facility") == 10);
    CHECK(faction.GetEconomy().GetEnergy() == 10);
    CHECK_FALSE(base.GetBuildingManager().HasBuilding("test_hurry_facility"));
    CHECK_FALSE(faction.QuoteScrapBuilding(base, "test_hurry_facility").has_value());
}

TEST_CASE("Destroying a building without scrap grants no energy", "[building][scrap]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 2, 2);
    base.GetBuildingManager().AddBuilding("test_hurry_facility");

    base.GetBuildingManager().DestroyBuilding("test_hurry_facility");
    faction.NotifyBuildingDestroyed(base.GetBaseId(), "test_hurry_facility");

    CHECK(faction.GetEconomy().GetEnergy() == 0);
    CHECK_FALSE(base.GetBuildingManager().HasBuilding("test_hurry_facility"));
}

TEST_CASE("Scrapping one allow-multiple copy leaves the others and refunds once",
          "[building][scrap]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 2, 2);
    base.GetBuildingManager().AddBuilding("test_hurry_facility");
    base.GetBuildingManager().AddBuilding("test_hurry_facility");

    CHECK(faction.QuoteScrapBuilding(base, "test_hurry_facility")->amount == 10);
    CHECK(faction.QuoteScrapBuildings("test_hurry_facility")->amount == 20);

    CHECK(faction.ScrapBuilding(base, "test_hurry_facility") == 10);
    CHECK(faction.GetEconomy().GetEnergy() == 10);
    CHECK(base.GetBuildingManager().HasBuilding("test_hurry_facility"));
    CHECK(base.GetBuildingManager().GetBuildings().size() == 1);
    CHECK(faction.QuoteScrapBuildings("test_hurry_facility")->amount == 10);
}

TEST_CASE("Scrapping a building type removes it from every base and refunds each copy",
          "[building][scrap]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& baseA = fixture.MakeFactionBase(faction, 2, 2);
    BaseManager& baseB = fixture.MakeFactionBase(faction, 5, 5);
    baseA.GetBuildingManager().AddBuilding("test_hurry_facility");
    baseB.GetBuildingManager().AddBuilding("test_hurry_facility");
    baseB.GetBuildingManager().AddBuilding("test_facility_b");
    REQUIRE(faction.QuoteScrapBuilding(baseA, "test_hurry_facility")->amount == 10);
    REQUIRE(faction.QuoteScrapBuilding(baseB, "test_hurry_facility")->amount == 10);
    REQUIRE(faction.QuoteScrapBuildings("test_hurry_facility")->amount == 20);

    CHECK(faction.ScrapBuildings("test_hurry_facility") == 20);
    CHECK(faction.GetEconomy().GetEnergy() == 20);
    CHECK_FALSE(baseA.GetBuildingManager().HasBuilding("test_hurry_facility"));
    CHECK_FALSE(baseB.GetBuildingManager().HasBuilding("test_hurry_facility"));
    CHECK(baseB.GetBuildingManager().HasBuilding("test_facility_b"));
    CHECK_FALSE(faction.QuoteScrapBuildings("test_hurry_facility").has_value());
    CHECK(faction.CountBuildings("test_hurry_facility") == 0);
}

TEST_CASE("Scrap of a building this base does not hold is an error", "[building][scrap]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 2, 2);

    CHECK_FALSE(faction.QuoteScrapBuilding(base, "test_hurry_facility").has_value());
    CHECK_FALSE(faction.QuoteScrapBuildings("test_hurry_facility").has_value());
    CHECK_THROWS_WITH(faction.ScrapBuilding(base, "test_hurry_facility"),
                      Catch::Matchers::ContainsSubstring("cannot be scrapped"));
    CHECK_THROWS_WITH(faction.ScrapBuildings("test_hurry_facility"),
                      Catch::Matchers::ContainsSubstring("cannot be scrapped"));
    CHECK(faction.GetEconomy().GetEnergy() == 0);
}

TEST_CASE("Scrap of a building at another faction's base is an error", "[building][scrap]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    Faction& other = fixture.MakeFaction();
    BaseManager& foreign = fixture.MakeFactionBase(other, 5, 5);
    foreign.GetBuildingManager().AddBuilding("test_hurry_facility");

    CHECK_THROWS_WITH(faction.QuoteScrapBuilding(foreign, "test_hurry_facility"),
                      Catch::Matchers::ContainsSubstring("not owned by this faction"));
    CHECK_THROWS_WITH(faction.ScrapBuilding(foreign, "test_hurry_facility"),
                      Catch::Matchers::ContainsSubstring("not owned by this faction"));
    CHECK(foreign.GetBuildingManager().HasBuilding("test_hurry_facility"));
    CHECK(faction.GetEconomy().GetEnergy() == 0);
    CHECK(other.GetEconomy().GetEnergy() == 0);
}

TEST_CASE("A secret project cannot be scrapped", "[building][scrap][secret-project]")
{
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
    Faction& faction = pState->AddFaction(std::make_unique<Faction>(
        pState->AllocateFactionId(), true, fixtures.factionDefinition, fixtures.dataContext,
        pState->GetWorldMap(), fixtures.settings, k_TestFactionSeed));
    BaseManager* pBase = faction.CreateBase(
        pState->AllocateBaseId(), "TestBase", pState->GetWorldMap().GetTile(2, 2),
        fixtures.dataContext, pState->GetTileEffects(),
        pState->GetSecretProjectAvailability());
    REQUIRE(pBase != nullptr);

    pBase->GetBuildingManager().AddBuilding("test_secret_project");
    CHECK_FALSE(faction.QuoteScrapBuilding(*pBase, "test_secret_project").has_value());
    CHECK_FALSE(faction.QuoteScrapBuildings("test_secret_project").has_value());
    CHECK_THROWS_WITH(faction.ScrapBuilding(*pBase, "test_secret_project"),
                      Catch::Matchers::ContainsSubstring("secret project"));
    CHECK_THROWS_WITH(faction.ScrapBuildings("test_secret_project"),
                      Catch::Matchers::ContainsSubstring("cannot be scrapped"));
    CHECK(pBase->GetBuildingManager().HasBuilding("test_secret_project"));
    CHECK(faction.GetEconomy().GetEnergy() == 0);
    CHECK(pState->GetSecretProjectAvailability().IsOwnedByAnyFaction("test_secret_project"));
    CHECK_FALSE(pState->IsSecretProjectDestroyed("test_secret_project"));
}

TEST_CASE("Headquarters cannot be scrapped", "[building][scrap][hq]")
{
    FactionFixture fixture;
    Faction& faction = fixture.MakeFaction();
    BaseManager& base = fixture.MakeFactionBase(faction, 2, 2);
    base.GetBuildingManager().AddBuilding("Headquarters");
    REQUIRE(base.GetBuildingManager().HasBuilding("Headquarters"));
    const BuildingConfig_t* pHq = fixture.buildings().Find("Headquarters");
    REQUIRE(pHq);
    REQUIRE(pHq->scrap.has_value());
    CHECK(ScrapOverrideDenies(*pHq->scrap));

    CHECK_FALSE(faction.QuoteScrapBuilding(base, "Headquarters").has_value());
    CHECK_FALSE(faction.QuoteScrapBuildings("Headquarters").has_value());
    CHECK_THROWS_WITH(faction.ScrapBuilding(base, "Headquarters"),
                      Catch::Matchers::ContainsSubstring("cannot be scrapped"));
    CHECK_THROWS_WITH(faction.ScrapBuildings("Headquarters"),
                      Catch::Matchers::ContainsSubstring("cannot be scrapped"));
    CHECK(base.GetBuildingManager().HasBuilding("Headquarters"));
    CHECK(faction.GetEconomy().GetEnergy() == 0);
}

TEST_CASE("Production config groups default_scrap under kinds", "[building][scrap][config]")
{
    SECTION("shipping shape loads")
    {
        const TempConfigFile file("scrap_ok.json", R"json({
            "retool_penalty_threshold": 10,
            "retool_penalty_percent": 50,
            "prototype_surcharge_percent": 50,
            "kinds": {
                "building": {
                    "default_scrap": {
                        "formula": "floor(minerals / 4)",
                        "refund_type": "energy_credits",
                        "refund_ceiling_percent": 100
                    }
                }
            }
        })json");
        const ProductionConfig_t config = ProductionConfigParser{}.ParseConfig(file.Path());
        REQUIRE(config.kinds.at(ConstructableKind_t::Building).defaultScrap.has_value());
        const ScrapKindConfig_t& rScrap = *config.kinds.at(ConstructableKind_t::Building).defaultScrap;
        CHECK(rScrap.formula == "floor(minerals / 4)");
        CHECK(rScrap.refundType == StatId_t::EnergyCredits);
        CHECK(rScrap.refundCeilingPercent == 100);
    }

    SECTION("omitting default_scrap on a kind leaves scrap disabled for it")
    {
        const TempConfigFile file("scrap_omitted.json", R"({
            "retool_penalty_threshold": 10,
            "retool_penalty_percent": 50,
            "prototype_surcharge_percent": 50,
            "kinds": {
                "building": {
                    "hurry": {
                        "formula": "1",
                        "mineral_threshold": 10,
                        "below_threshold_multiplier": 2
                    }
                }
            }
        })");
        const ProductionConfig_t config = ProductionConfigParser{}.ParseConfig(file.Path());
        CHECK_FALSE(config.kinds.at(ConstructableKind_t::Building).defaultScrap.has_value());
    }

    SECTION("a kind missing formula throws")
    {
        const TempConfigFile file("scrap_no_formula.json", R"({
            "retool_penalty_threshold": 10,
            "retool_penalty_percent": 50,
            "prototype_surcharge_percent": 50,
            "kinds": {
                "building": {
                    "default_scrap": {
                        "refund_type": "energy_credits",
                        "refund_ceiling_percent": 100
                    }
                }
            }
        })");
        CHECK_THROWS_WITH(ProductionConfigParser{}.ParseConfig(file.Path()),
                          Catch::Matchers::ContainsSubstring("kinds.building.default_scrap")
                              && Catch::Matchers::ContainsSubstring("formula"));
    }

    SECTION("a kind missing refund_type throws")
    {
        const TempConfigFile file("scrap_no_type.json", R"json({
            "retool_penalty_threshold": 10,
            "retool_penalty_percent": 50,
            "prototype_surcharge_percent": 50,
            "kinds": {
                "building": {
                    "default_scrap": {
                        "formula": "floor(minerals / 2)",
                        "refund_ceiling_percent": 100
                    }
                }
            }
        })json");
        CHECK_THROWS_WITH(ProductionConfigParser{}.ParseConfig(file.Path()),
                          Catch::Matchers::ContainsSubstring("kinds.building.default_scrap")
                              && Catch::Matchers::ContainsSubstring("refund_type"));
    }

    SECTION("a kind missing refund_ceiling_percent throws")
    {
        const TempConfigFile file("scrap_no_ceiling.json", R"json({
            "retool_penalty_threshold": 10,
            "retool_penalty_percent": 50,
            "prototype_surcharge_percent": 50,
            "kinds": {
                "building": {
                    "default_scrap": {
                        "formula": "floor(minerals / 2)",
                        "refund_type": "energy_credits"
                    }
                }
            }
        })json");
        CHECK_THROWS_WITH(ProductionConfigParser{}.ParseConfig(file.Path()),
                          Catch::Matchers::ContainsSubstring("kinds.building.default_scrap")
                              && Catch::Matchers::ContainsSubstring("refund_ceiling_percent"));
    }

    SECTION("an unknown refund_type throws")
    {
        const TempConfigFile file("scrap_bad_type.json", R"({
            "retool_penalty_threshold": 10,
            "retool_penalty_percent": 50,
            "prototype_surcharge_percent": 50,
            "kinds": {
                "building": {
                    "default_scrap": {
                        "formula": "1",
                        "refund_type": "gold",
                        "refund_ceiling_percent": 100
                    }
                }
            }
        })");
        CHECK_THROWS_WITH(ProductionConfigParser{}.ParseConfig(file.Path()),
                          Catch::Matchers::ContainsSubstring("refund_type"));
    }

    SECTION("a non-payout stat as refund_type throws")
    {
        const TempConfigFile file("scrap_attack.json", R"({
            "retool_penalty_threshold": 10,
            "retool_penalty_percent": 50,
            "prototype_surcharge_percent": 50,
            "kinds": {
                "building": {
                    "default_scrap": {
                        "formula": "1",
                        "refund_type": "attack",
                        "refund_ceiling_percent": 100
                    }
                }
            }
        })");
        CHECK_THROWS_WITH(ProductionConfigParser{}.ParseConfig(file.Path()),
                          Catch::Matchers::ContainsSubstring("creditable scrap payout"));
    }

    SECTION("nutrients is a legal refund_type")
    {
        const TempConfigFile file("scrap_nutrients.json", R"json({
            "retool_penalty_threshold": 10,
            "retool_penalty_percent": 50,
            "prototype_surcharge_percent": 50,
            "kinds": {
                "building": {
                    "default_scrap": {
                        "formula": "1",
                        "refund_type": "nutrients",
                        "refund_ceiling_percent": 100
                    }
                }
            }
        })json");
        const ProductionConfig_t config = ProductionConfigParser{}.ParseConfig(file.Path());
        CHECK(config.kinds.at(ConstructableKind_t::Building).defaultScrap->refundType
              == StatId_t::Nutrients);
    }

    SECTION("secret_project cannot have a default_scrap block")
    {
        const TempConfigFile file("scrap_sp.json", R"json({
            "retool_penalty_threshold": 10,
            "retool_penalty_percent": 50,
            "prototype_surcharge_percent": 50,
            "kinds": {
                "secret_project": {
                    "default_scrap": {
                        "formula": "floor(minerals / 2)",
                        "refund_type": "energy_credits",
                        "refund_ceiling_percent": 100
                    }
                }
            }
        })json");
        CHECK_THROWS_WITH(ProductionConfigParser{}.ParseConfig(file.Path()),
                          Catch::Matchers::ContainsSubstring("kinds.secret_project.default_scrap")
                              && Catch::Matchers::ContainsSubstring("cannot be scrapped"));
    }
}

TEST_CASE("A scrap override may clear the formula to deny scrap", "[building][scrap][config]")
{
    const ScrapOverride_t deny =
        ScrapConfigParser::ParseOverride(nlohmann::json::parse(R"({"formula": null})"), "test");
    CHECK(ScrapOverrideDenies(deny));
    CHECK(deny.formula == "");

    CHECK_THROWS_WITH(
        ScrapConfigParser::ParseOverride(
            nlohmann::json::parse(R"({"formula": null, "refund_type": "minerals"})"), "test"),
        Catch::Matchers::ContainsSubstring("null formula"));
}
