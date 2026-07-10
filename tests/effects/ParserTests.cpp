// Tests for BonusEffectParser — the single shared implementation of the JSON `effects`
// array schema used by every config source (buildings, unit components, pop types,
// improvements, social policies).

#include "game/effects/BonusEffectParser.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace ac;
using Catch::Approx;
using nlohmann::json;

TEST_CASE("ParseStatId: canonical string mappings", "[effects][parser]")
{
    CHECK(BonusEffectParser::ParseStatId("nutrients") == StatId_t::Nutrients);
    CHECK(BonusEffectParser::ParseStatId("minerals") == StatId_t::Minerals);
    CHECK(BonusEffectParser::ParseStatId("energy") == StatId_t::Energy);
    CHECK(BonusEffectParser::ParseStatId("econ") == StatId_t::Econ);
    CHECK(BonusEffectParser::ParseStatId("labs") == StatId_t::Labs);
    CHECK(BonusEffectParser::ParseStatId("psych") == StatId_t::Psych);
    CHECK(BonusEffectParser::ParseStatId("attack") == StatId_t::Attack);
    CHECK(BonusEffectParser::ParseStatId("defense") == StatId_t::Defense);
    CHECK(BonusEffectParser::ParseStatId("movement") == StatId_t::Movement);
    CHECK(BonusEffectParser::ParseStatId("hit_points") == StatId_t::HitPoints);
    CHECK(BonusEffectParser::ParseStatId("disengage_chance") == StatId_t::DisengageChance);
    CHECK(BonusEffectParser::ParseStatId("fuel") == StatId_t::Fuel);
    CHECK(BonusEffectParser::ParseStatId("damage_from_out_of_fuel") == StatId_t::DamageFromOutOfFuel);
    CHECK(BonusEffectParser::ParseStatId("cargo_capacity") == StatId_t::CargoCapacity);
    CHECK(BonusEffectParser::ParseStatId("difficult_terrain_cost") == StatId_t::DifficultTerrainCost);
    CHECK(BonusEffectParser::ParseStatId("cost_multiplier") == StatId_t::CostMultiplier);
    CHECK(BonusEffectParser::ParseStatId("growth_rate") == StatId_t::GrowthRate);
    CHECK(BonusEffectParser::ParseStatId("moisture_tier") == StatId_t::MoistureTier);

    CHECK_THROWS(BonusEffectParser::ParseStatId("not_a_stat"));
    CHECK_THROWS(BonusEffectParser::ParseStatId(""));
    // Ids are case-sensitive.
    CHECK_THROWS(BonusEffectParser::ParseStatId("Nutrients"));
}

TEST_CASE("ParseModifierOp / ParseEffectScope / ParseEffectPersistence mappings", "[effects][parser]")
{
    CHECK(BonusEffectParser::ParseModifierOp("Add") == ModifierOp_t::Add);
    CHECK(BonusEffectParser::ParseModifierOp("AddPercent") == ModifierOp_t::AddPercent);
    CHECK(BonusEffectParser::ParseModifierOp("MultiplyGeometric") == ModifierOp_t::MultiplyGeometric);
    CHECK_THROWS(BonusEffectParser::ParseModifierOp("Multiply"));

    CHECK(BonusEffectParser::ParseEffectScope("ThisBase") == EffectScope_t::ThisBase);
    CHECK(BonusEffectParser::ParseEffectScope("AllOwnerBases") == EffectScope_t::AllOwnerBases);
    CHECK(BonusEffectParser::ParseEffectScope("ThisUnit") == EffectScope_t::ThisUnit);
    CHECK(BonusEffectParser::ParseEffectScope("FactionUnits") == EffectScope_t::FactionUnits);
    CHECK(BonusEffectParser::ParseEffectScope("FactionGlobal") == EffectScope_t::FactionGlobal);
    CHECK(BonusEffectParser::ParseEffectScope("WorldGlobal") == EffectScope_t::WorldGlobal);
    CHECK(BonusEffectParser::ParseEffectScope("ThisPop") == EffectScope_t::ThisPop);
    CHECK(BonusEffectParser::ParseEffectScope("ThisTile") == EffectScope_t::ThisTile);
    CHECK_THROWS(BonusEffectParser::ParseEffectScope("Global"));

    CHECK(BonusEffectParser::ParseEffectPersistence("Instantaneous") == EffectPersistence_t::Instantaneous);
    CHECK(BonusEffectParser::ParseEffectPersistence("Continuous") == EffectPersistence_t::Continuous);
    CHECK_THROWS(BonusEffectParser::ParseEffectPersistence("Permanent"));
}

TEST_CASE("ParseRuleFlagId and ParseSocialRatingId mappings", "[effects][parser]")
{
    CHECK(BonusEffectParser::ParseRuleFlagId("flight") == RuleFlagId_t::Flight);
    CHECK(BonusEffectParser::ParseRuleFlagId("single_use") == RuleFlagId_t::SingleUse);
    CHECK(BonusEffectParser::ParseRuleFlagId("population_boom") == RuleFlagId_t::PopulationBoom);
    CHECK(BonusEffectParser::ParseRuleFlagId("near_zero_growth") == RuleFlagId_t::NearZeroGrowth);
    CHECK_THROWS(BonusEffectParser::ParseRuleFlagId("hover"));

    CHECK(BonusEffectParser::ParseSocialRatingId("economy") == SocialRatingId_t::Economy);
    CHECK(BonusEffectParser::ParseSocialRatingId("efficiency") == SocialRatingId_t::Efficiency);
    CHECK(BonusEffectParser::ParseSocialRatingId("support") == SocialRatingId_t::Support);
    CHECK(BonusEffectParser::ParseSocialRatingId("police") == SocialRatingId_t::Police);
    CHECK(BonusEffectParser::ParseSocialRatingId("morale") == SocialRatingId_t::Morale);
    CHECK(BonusEffectParser::ParseSocialRatingId("growth") == SocialRatingId_t::Growth);
    CHECK(BonusEffectParser::ParseSocialRatingId("planet") == SocialRatingId_t::Planet);
    CHECK(BonusEffectParser::ParseSocialRatingId("research") == SocialRatingId_t::Research);
    CHECK(BonusEffectParser::ParseSocialRatingId("industry") == SocialRatingId_t::Industry);
    CHECK(BonusEffectParser::ParseSocialRatingId("probe") == SocialRatingId_t::Probe);
    CHECK_THROWS(BonusEffectParser::ParseSocialRatingId("karma"));
}

TEST_CASE("ParseNumber: accepts numbers and numeric strings", "[effects][parser]")
{
    const json params = {{"as_number", 2.5}, {"as_int", 3}, {"as_string", "4.5"}, {"bad", true}};

    CHECK(BonusEffectParser::ParseNumber(params, "as_number", 0.0) == Approx(2.5));
    CHECK(BonusEffectParser::ParseNumber(params, "as_int", 0.0) == Approx(3.0));
    CHECK(BonusEffectParser::ParseNumber(params, "as_string", 0.0) == Approx(4.5));
    CHECK(BonusEffectParser::ParseNumber(params, "missing", 7.0) == Approx(7.0));
    CHECK_THROWS(BonusEffectParser::ParseNumber(params, "bad", 0.0));
}

TEST_CASE("ParseEffectConfig: StatModifier with explicit fields", "[effects][parser]")
{
    const json effectJson = json::parse(R"({
        "type": "StatModifier",
        "scope": "ThisBase",
        "persistence": "Continuous",
        "parameters": { "stat": "minerals", "amount": 3, "op": "AddPercent" }
    })");

    const EffectConfig_t config = BonusEffectParser::ParseEffectConfig(effectJson);
    CHECK(config.scope == EffectScope_t::ThisBase);
    CHECK(config.persistence == EffectPersistence_t::Continuous);
    CHECK_FALSE(config.condition.has_value());

    const auto* pMod = std::get_if<StatModifierEffect_t>(&config.effect);
    REQUIRE(pMod != nullptr);
    CHECK(pMod->stat == StatId_t::Minerals);
    CHECK(pMod->amount == Approx(3.0));
    CHECK(pMod->op == ModifierOp_t::AddPercent);
    CHECK_FALSE(pMod->selector.has_value());
}

TEST_CASE("ParseEffectConfig: defaults — persistence Continuous, op Add, amount 0", "[effects][parser]")
{
    const json effectJson = json::parse(R"({
        "type": "StatModifier",
        "scope": "ThisTile",
        "parameters": { "stat": "energy" }
    })");

    const EffectConfig_t config = BonusEffectParser::ParseEffectConfig(effectJson);
    CHECK(config.persistence == EffectPersistence_t::Continuous);

    const auto* pMod = std::get_if<StatModifierEffect_t>(&config.effect);
    REQUIRE(pMod != nullptr);
    CHECK(pMod->op == ModifierOp_t::Add);
    CHECK(pMod->amount == Approx(0.0));
}

TEST_CASE("ParseEffectConfig: amount as numeric string (used by real configs)", "[effects][parser]")
{
    const json effectJson = json::parse(R"({
        "type": "StatModifier",
        "scope": "ThisBase",
        "parameters": { "stat": "nutrients", "amount": "2" }
    })");

    const EffectConfig_t config = BonusEffectParser::ParseEffectConfig(effectJson);
    const auto* pMod = std::get_if<StatModifierEffect_t>(&config.effect);
    REQUIRE(pMod != nullptr);
    CHECK(pMod->amount == Approx(2.0));
}

TEST_CASE("ParseEffectConfig: StatModifier tile selectors", "[effects][parser]")
{
    SECTION("HasImprovement selector")
    {
        const json effectJson = json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisBase",
            "parameters": {
                "stat": "nutrients", "amount": 1,
                "selector": { "kind": "HasImprovement", "improvement": "Farm" }
            }
        })");

        const EffectConfig_t config = BonusEffectParser::ParseEffectConfig(effectJson);
        const auto* pMod = std::get_if<StatModifierEffect_t>(&config.effect);
        REQUIRE(pMod != nullptr);
        REQUIRE(pMod->selector.has_value());
        CHECK(pMod->selector->kind == TileSelectorKind_t::HasImprovement);
        REQUIRE(pMod->selector->improvement.has_value());
        CHECK(*pMod->selector->improvement == "Farm");
    }

    SECTION("BaseTile selector (also the default kind)")
    {
        const json effectJson = json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisBase",
            "parameters": { "stat": "energy", "amount": 2, "selector": { "kind": "BaseTile" } }
        })");

        const EffectConfig_t config = BonusEffectParser::ParseEffectConfig(effectJson);
        const auto* pMod = std::get_if<StatModifierEffect_t>(&config.effect);
        REQUIRE(pMod != nullptr);
        REQUIRE(pMod->selector.has_value());
        CHECK(pMod->selector->kind == TileSelectorKind_t::BaseTile);
        CHECK_FALSE(pMod->selector->improvement.has_value());
    }

    SECTION("HasImprovement without an improvement id throws")
    {
        const json selectorJson = json::parse(R"({ "kind": "HasImprovement" })");
        CHECK_THROWS(BonusEffectParser::ParseTileSelector(selectorJson));
    }

    SECTION("unknown selector kind throws")
    {
        const json selectorJson = json::parse(R"({ "kind": "Everything" })");
        CHECK_THROWS(BonusEffectParser::ParseTileSelector(selectorJson));
    }

    SECTION("selector on a non-tile-resource stat throws")
    {
        // Selectors are resolved only during tile-yield resolution (nutrients/minerals/
        // energy); on any other stat the modifier would silently never apply.
        for (const char* stat : {"econ", "defense", "growth_rate"})
        {
            const json effectJson = json::parse(std::string(R"({
                "type": "StatModifier",
                "scope": "ThisBase",
                "parameters": {
                    "stat": ")") + stat + R"(", "amount": 1,
                    "selector": { "kind": "BaseTile" }
                }
            })");
            CHECK_THROWS(BonusEffectParser::ParseEffectConfig(effectJson));
        }
    }
}

TEST_CASE("ParseEffectConfig: conditions", "[effects][parser][condition]")
{
    SECTION("TargetTileHas condition")
    {
        const json effectJson = json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisUnit",
            "condition": { "kind": "TargetTileHas", "value": "Forest" },
            "parameters": { "stat": "attack", "amount": 25, "op": "AddPercent" }
        })");

        const EffectConfig_t config = BonusEffectParser::ParseEffectConfig(effectJson);
        REQUIRE(config.condition.has_value());
        CHECK(config.condition->kind == ConditionKind_t::TargetTileHas);
        CHECK(config.condition->value == "Forest");
    }

    SECTION("empty condition value throws")
    {
        CHECK_THROWS(BonusEffectParser::ParseCondition(json::parse(R"({ "kind": "TargetTileHas" })")));
    }

    SECTION("unknown condition kind throws")
    {
        CHECK_THROWS(BonusEffectParser::ParseCondition(
            json::parse(R"({ "kind": "TargetIsShiny", "value": "x" })")));
    }
}

TEST_CASE("ParseEffectConfig: grant effects require their id parameter", "[effects][parser]")
{
    const json grantBuilding = json::parse(R"({
        "type": "GrantBuilding", "scope": "ThisBase",
        "parameters": { "building_id": "network_node" }
    })");
    const EffectConfig_t grantConfig = BonusEffectParser::ParseEffectConfig(grantBuilding);
    const auto* pGrant = std::get_if<GrantBuildingEffect_t>(&grantConfig.effect);
    REQUIRE(pGrant != nullptr);
    CHECK(pGrant->buildingId == "network_node");

    CHECK_THROWS(BonusEffectParser::ParseEffectConfig(
        json::parse(R"({ "type": "GrantBuilding", "scope": "ThisBase", "parameters": {} })")));
    CHECK_THROWS(BonusEffectParser::ParseEffectConfig(
        json::parse(R"({ "type": "GrantTech", "scope": "FactionGlobal", "parameters": {} })")));
    CHECK_THROWS(BonusEffectParser::ParseEffectConfig(
        json::parse(R"({ "type": "GrantUnit", "scope": "FactionGlobal", "parameters": {} })")));

    const json grantTech = json::parse(R"({
        "type": "GrantTech", "scope": "FactionGlobal",
        "persistence": "Instantaneous",
        "parameters": { "tech_id": "biogenetics" }
    })");
    const EffectConfig_t techConfig = BonusEffectParser::ParseEffectConfig(grantTech);
    CHECK(techConfig.persistence == EffectPersistence_t::Instantaneous);
    const auto* pTech = std::get_if<GrantTechEffect_t>(&techConfig.effect);
    REQUIRE(pTech != nullptr);
    CHECK(pTech->techId == "biogenetics");
}

TEST_CASE("ParseEffectConfig: RuleFlag requires a valid flag", "[effects][parser]")
{
    const json flagJson = json::parse(R"({
        "type": "RuleFlag", "scope": "ThisUnit", "parameters": { "flag": "flight" }
    })");
    const EffectConfig_t flagConfig = BonusEffectParser::ParseEffectConfig(flagJson);
    const auto* pFlag = std::get_if<RuleFlagEffect_t>(&flagConfig.effect);
    REQUIRE(pFlag != nullptr);
    CHECK(pFlag->flag == RuleFlagId_t::Flight);

    CHECK_THROWS(BonusEffectParser::ParseEffectConfig(
        json::parse(R"({ "type": "RuleFlag", "scope": "ThisUnit", "parameters": {} })")));
}

TEST_CASE("ParseEffectConfig: SocialRatingModifier", "[effects][parser]")
{
    const json ratingJson = json::parse(R"({
        "type": "SocialRatingModifier", "scope": "FactionGlobal",
        "parameters": { "rating": "police", "amount": -2 }
    })");
    const EffectConfig_t ratingConfig = BonusEffectParser::ParseEffectConfig(ratingJson);
    const auto* pRating = std::get_if<SocialRatingModifierEffect_t>(&ratingConfig.effect);
    REQUIRE(pRating != nullptr);
    CHECK(pRating->rating == SocialRatingId_t::Police);
    CHECK(pRating->amount == -2);

    CHECK_THROWS(BonusEffectParser::ParseEffectConfig(
        json::parse(R"({ "type": "SocialRatingModifier", "scope": "FactionGlobal", "parameters": {} })")));
}

TEST_CASE("ParseEffectConfig: unknown effect type throws", "[effects][parser]")
{
    CHECK_THROWS(BonusEffectParser::ParseEffectConfig(
        json::parse(R"({ "type": "MindControl", "scope": "WorldGlobal", "parameters": {} })")));
}

TEST_CASE("ParseEffectConfig: per-effect radius", "[effects][parser][radius]")
{
    SECTION("parsed from the effect entry, default 0")
    {
        const json withRadius = json::parse(R"({
            "type": "StatModifier", "scope": "ThisTile", "radius": 2,
            "parameters": { "stat": "energy", "amount": 1 }
        })");
        CHECK(BonusEffectParser::ParseEffectConfig(withRadius).radius == 2);

        const json withoutRadius = json::parse(R"({
            "type": "StatModifier", "scope": "ThisTile",
            "parameters": { "stat": "energy", "amount": 1 }
        })");
        CHECK(BonusEffectParser::ParseEffectConfig(withoutRadius).radius == 0);
    }

    SECTION("negative radius throws")
    {
        const json negative = json::parse(R"({
            "type": "StatModifier", "scope": "ThisTile", "radius": -1,
            "parameters": { "stat": "energy", "amount": 1 }
        })");
        CHECK_THROWS(BonusEffectParser::ParseEffectConfig(negative));
    }
}

TEST_CASE("ValidateScopeForSource: rejects only the certainly-impossible combinations",
          "[effects][parser][validation]")
{
    // ThisPop can only ever resolve against a pop type; ThisUnit against a unit component.
    CHECK_THROWS(BonusEffectParser::ValidateScopeForSource(
        EffectScope_t::ThisPop, EffectSourceKind_t::Building, "some_building"));
    CHECK_THROWS(BonusEffectParser::ValidateScopeForSource(
        EffectScope_t::ThisUnit, EffectSourceKind_t::PopType, "some_pop"));

    CHECK_NOTHROW(BonusEffectParser::ValidateScopeForSource(
        EffectScope_t::ThisPop, EffectSourceKind_t::PopType, "some_pop"));
    CHECK_NOTHROW(BonusEffectParser::ValidateScopeForSource(
        EffectScope_t::ThisUnit, EffectSourceKind_t::UnitComponent, "some_component"));

    // Everything else loads — including combinations whose anchor concept doesn't exist yet
    // (e.g. a faction-lane effect on an improvement, pending territory ownership).
    CHECK_NOTHROW(BonusEffectParser::ValidateScopeForSource(
        EffectScope_t::FactionGlobal, EffectSourceKind_t::Improvement, "monolith"));
    CHECK_NOTHROW(BonusEffectParser::ValidateScopeForSource(
        EffectScope_t::ThisTile, EffectSourceKind_t::UnitComponent, "sensor_pod"));
    CHECK_NOTHROW(BonusEffectParser::ValidateScopeForSource(
        EffectScope_t::WorldGlobal, EffectSourceKind_t::Building, "beacon"));
}

TEST_CASE("ParseEffects with a source kind validates every entry", "[effects][parser][validation]")
{
    const json badContainer = json::parse(R"({
        "id": "bad_building",
        "effects": [
            { "type": "StatModifier", "scope": "ThisPop", "parameters": { "stat": "econ", "amount": 1 } }
        ]
    })");
    CHECK_THROWS(BonusEffectParser::ParseEffects(badContainer, EffectSourceKind_t::Building, "bad_building"));
    CHECK_NOTHROW(BonusEffectParser::ParseEffects(badContainer, EffectSourceKind_t::PopType, "fine_as_pop"));
}

TEST_CASE("ParseEffects: absent effects array yields empty vector; entries parse in order", "[effects][parser]")
{
    CHECK(BonusEffectParser::ParseEffects(json::parse(R"({ "id": "no_effects" })")).empty());

    const json container = json::parse(R"({
        "id": "two_effects",
        "effects": [
            { "type": "StatModifier", "scope": "ThisBase", "parameters": { "stat": "nutrients", "amount": 1 } },
            { "type": "RuleFlag", "scope": "ThisUnit", "parameters": { "flag": "single_use" } }
        ]
    })");

    const std::vector<EffectConfig_t> effects = BonusEffectParser::ParseEffects(container);
    REQUIRE(effects.size() == 2);
    CHECK(std::holds_alternative<StatModifierEffect_t>(effects[0].effect));
    CHECK(std::holds_alternative<RuleFlagEffect_t>(effects[1].effect));
}
