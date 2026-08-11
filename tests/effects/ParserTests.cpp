// Tests for EffectConfigParser — the single shared implementation of the JSON `effects`
// array schema used by every config source (buildings, unit components, pop types,
// improvements, social policies).

#include "game/effects/EffectConfig.h"
#include "game/effects/EffectConfigParser.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <variant>

using namespace ac;
using Catch::Approx;
using nlohmann::json;

TEST_CASE("ParseStatId: canonical string mappings", "[effects][parser]")
{
    CHECK(ParseStatId("nutrients") == StatId_t::Nutrients);
    CHECK(ParseStatId("minerals") == StatId_t::Minerals);
    CHECK(ParseStatId("energy") == StatId_t::Energy);
    CHECK(ParseStatId("econ") == StatId_t::Econ);
    CHECK(ParseStatId("labs") == StatId_t::Labs);
    CHECK(ParseStatId("psych") == StatId_t::Psych);
    CHECK(ParseStatId("drones") == StatId_t::Drones);
    CHECK(ParseStatId("talents") == StatId_t::Talents);
    CHECK(ParseStatId("attack") == StatId_t::Attack);
    CHECK(ParseStatId("defense") == StatId_t::Defense);
    CHECK(ParseStatId("movement") == StatId_t::Movement);
    CHECK(ParseStatId("vision") == StatId_t::Vision);
    CHECK(ParseStatId("hit_points") == StatId_t::HitPoints);
    CHECK(ParseStatId("psi_damage") == StatId_t::PsiDamage);
    CHECK(ParseStatId("disengage_chance") == StatId_t::DisengageChance);
    CHECK(ParseStatId("turns_of_fuel") == StatId_t::TurnsOfFuel);
    CHECK(ParseStatId("damage_from_out_of_fuel") == StatId_t::DamageFromOutOfFuel);
    CHECK(ParseStatId("cargo_capacity") == StatId_t::CargoCapacity);
    CHECK(ParseStatId("difficult_terrain_cost")
          == StatId_t::DifficultTerrainCost);
    CHECK(ParseStatId("mineral_upkeep") == StatId_t::MineralUpkeep);
    CHECK(ParseStatId("free_unit_support") == StatId_t::FreeUnitSupport);
    CHECK(ParseStatId("cost_multiplier") == StatId_t::CostMultiplier);
    CHECK(ParseStatId("facility_energy_upkeep") == StatId_t::FacilityEnergyUpkeep);
    CHECK(ParseStatId("probe_action_cost") == StatId_t::ProbeActionCost);
    CHECK(ParseStatId("probe_defense") == StatId_t::ProbeDefense);
    CHECK(ParseStatId("probe_failure_scale") == StatId_t::ProbeFailureScale);
    CHECK(ParseStatId("probe_success_scale") == StatId_t::ProbeSuccessScale);
    CHECK(ParseStatId("starting_experience") == StatId_t::StartingExperience);
    CHECK(ParseStatId("starting_minerals") == StatId_t::StartingMinerals);
    CHECK(ParseStatId("morale_bonus") == StatId_t::MoraleBonus);
    CHECK(ParseStatId("positive_morale_scale") == StatId_t::PositiveMoraleScale);
    CHECK(ParseStatId("growth_rate") == StatId_t::GrowthRate);
    CHECK(ParseStatId("tech_cost") == StatId_t::TechCost);
    CHECK(ParseStatId("moisture_tier") == StatId_t::MoistureTier);
    CHECK(ParseStatId("commerce_rate") == StatId_t::CommerceRate);
    CHECK(ParseStatId("council_votes") == StatId_t::CouncilVotes);
    CHECK(ParseStatId("commerce_energy_bonus") == StatId_t::CommerceEnergyBonus);
    CHECK(ParseStatId("inefficiency_denominator") == StatId_t::InefficiencyDenominator);

    CHECK_THROWS(ParseStatId("not_a_stat"));
    CHECK_THROWS(ParseStatId(""));
    // Ids are case-sensitive.
    CHECK_THROWS(ParseStatId("Nutrients"));
}

TEST_CASE("ParseModifierOp / ParseEffectScope / ParseEffectPersistence mappings", "[effects][parser]")
{
    CHECK(EffectConfigParser::ParseModifierOp("Add") == ModifierOp_t::Add);
    CHECK(EffectConfigParser::ParseModifierOp("AddPercent") == ModifierOp_t::AddPercent);
    CHECK(EffectConfigParser::ParseModifierOp("MultiplyGeometric") == ModifierOp_t::MultiplyGeometric);
    CHECK_THROWS(EffectConfigParser::ParseModifierOp("Multiply"));

    CHECK(EffectConfigParser::ParseEffectScope("ThisBase") == EffectScope_t::ThisBase);
    CHECK(EffectConfigParser::ParseEffectScope("AllOwnerBases") == EffectScope_t::AllOwnerBases);
    CHECK(EffectConfigParser::ParseEffectScope("ThisUnit") == EffectScope_t::ThisUnit);
    CHECK(EffectConfigParser::ParseEffectScope("FactionUnits") == EffectScope_t::FactionUnits);
    CHECK(EffectConfigParser::ParseEffectScope("ProducedAtThisBase")
          == EffectScope_t::ProducedAtThisBase);
    CHECK(EffectConfigParser::ParseEffectScope("FactionGlobal") == EffectScope_t::FactionGlobal);
    CHECK(EffectConfigParser::ParseEffectScope("WorldGlobal") == EffectScope_t::WorldGlobal);
    CHECK(EffectConfigParser::ParseEffectScope("ThisPop") == EffectScope_t::ThisPop);
    CHECK(EffectConfigParser::ParseEffectScope("ThisTile") == EffectScope_t::ThisTile);
    CHECK_THROWS(EffectConfigParser::ParseEffectScope("Global"));

    CHECK(EffectConfigParser::ParseEffectPersistence("Instantaneous") == EffectPersistence_t::Instantaneous);
    CHECK(EffectConfigParser::ParseEffectPersistence("Continuous") == EffectPersistence_t::Continuous);
    CHECK_THROWS(EffectConfigParser::ParseEffectPersistence("Permanent"));
}

TEST_CASE("ParseRuleFlagId and ParseSocialRatingId mappings", "[effects][parser]")
{
    CHECK(ParseRuleFlagId("single_use") == RuleFlagId_t::SingleUse);
    CHECK(ParseRuleFlagId("ignore_zone_of_control") == RuleFlagId_t::IgnoreZoneOfControl);
    CHECK(ParseRuleFlagId("population_boom") == RuleFlagId_t::PopulationBoom);
    CHECK(ParseRuleFlagId("near_zero_growth") == RuleFlagId_t::NearZeroGrowth);
    CHECK(ParseRuleFlagId("ignores_difficult_terrain") == RuleFlagId_t::IgnoreDifficultTerrain);
    CHECK(ParseRuleFlagId("treat_fungus_as_road") == RuleFlagId_t::TreatFungusAsRoad);
    CHECK(ParseRuleFlagId("forces_psi_combat") == RuleFlagId_t::ForcesPsiCombat);
    CHECK(ParseRuleFlagId("found_base") == RuleFlagId_t::FoundBase);
    CHECK(ParseRuleFlagId("terraform") == RuleFlagId_t::Terraform);
    CHECK(ParseRuleFlagId("supply_crawl") == RuleFlagId_t::SupplyCrawl);
    CHECK(ParseRuleFlagId("probe_team") == RuleFlagId_t::ProbeTeam);
    CHECK(ParseRuleFlagId("cannot_capture_bases")
          == RuleFlagId_t::CannotCaptureBases);
    CHECK(ParseRuleFlagId("attacking_ends_turn")
          == RuleFlagId_t::AttackingEndsTurn);
    CHECK(ParseRuleFlagId("no_conquest_repair")
          == RuleFlagId_t::NoConquestRepair);
    CHECK(ParseRuleFlagId("prevents_conquest_pop_loss")
          == RuleFlagId_t::PreventsConquestPopLoss);
    CHECK(ParseRuleFlagId("headquarters") == RuleFlagId_t::Headquarters);
    CHECK(ParseRuleFlagId("probe_subversion_immune")
          == RuleFlagId_t::ProbeSubversionImmune);
    CHECK(ParseRuleFlagId("blocks_probe_teams")
          == RuleFlagId_t::BlocksProbeTeams);
    CHECK(ParseRuleFlagId("ignores_probe_block")
          == RuleFlagId_t::IgnoresProbeBlock);
    CHECK(ParseRuleFlagId("creche") == RuleFlagId_t::Creche);
    CHECK(ParseRuleFlagId("prevents_disengage") == RuleFlagId_t::PreventsDisengage);
    CHECK(ParseRuleFlagId("remove_shroud") == RuleFlagId_t::RemoveShroud);
    CHECK(ParseRuleFlagId("remove_fog") == RuleFlagId_t::RemoveFog);
    CHECK(ParseRuleFlagId("atrocities_forbidden")
          == RuleFlagId_t::AtrocitiesForbidden);
    CHECK_THROWS(ParseRuleFlagId("hover"));
    CHECK_THROWS(ParseRuleFlagId("sea"));

    CHECK(ParseSocialRatingId("economy") == SocialRatingId_t::Economy);
    CHECK(ParseSocialRatingId("efficiency") == SocialRatingId_t::Efficiency);
    CHECK(ParseSocialRatingId("support") == SocialRatingId_t::Support);
    CHECK(ParseSocialRatingId("police") == SocialRatingId_t::Police);
    CHECK(ParseSocialRatingId("morale") == SocialRatingId_t::Morale);
    CHECK(ParseSocialRatingId("growth") == SocialRatingId_t::Growth);
    CHECK(ParseSocialRatingId("planet") == SocialRatingId_t::Planet);
    CHECK(ParseSocialRatingId("research") == SocialRatingId_t::Research);
    CHECK(ParseSocialRatingId("industry") == SocialRatingId_t::Industry);
    CHECK(ParseSocialRatingId("probe") == SocialRatingId_t::Probe);
    CHECK_THROWS(ParseSocialRatingId("karma"));
}

TEST_CASE("ParseNumber: accepts numbers and numeric strings", "[effects][parser]")
{
    const json params = {
        {"as_number", 2.5},
        {"as_int", 3},
        {"as_string", "4.5"},
        {"bad", true},
        {"junk", "2abc"},
    };

    CHECK(EffectConfigParser::ParseNumber(params, "as_number", 0.0) == Approx(2.5));
    CHECK(EffectConfigParser::ParseNumber(params, "as_int", 0.0) == Approx(3.0));
    CHECK(EffectConfigParser::ParseNumber(params, "as_string", 0.0) == Approx(4.5));
    CHECK(EffectConfigParser::ParseNumber(params, "missing", 7.0) == Approx(7.0));
    CHECK_THROWS(EffectConfigParser::ParseNumber(params, "bad", 0.0));

    try
    {
        EffectConfigParser::ParseNumber(params, "junk", 0.0);
        FAIL("expected trailing junk to throw");
    }
    catch (const std::runtime_error& e)
    {
        CHECK(std::string(e.what()).find("junk") != std::string::npos);
    }
}

TEST_CASE("ParseEffectConfig: StatModifier with explicit fields", "[effects][parser]")
{
    const json effectJson = json::parse(R"({
        "type": "StatModifier",
        "scope": "ThisBase",
        "persistence": "Continuous",
        "parameters": { "stat": "minerals", "amount": 3, "op": "AddPercent" }
    })");

    const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
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

    const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
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

    const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
    const auto* pMod = std::get_if<StatModifierEffect_t>(&config.effect);
    REQUIRE(pMod != nullptr);
    CHECK(pMod->amount == Approx(2.0));
}

TEST_CASE("ParseEffectConfig: StatModifier amount_source", "[effects][parser]")
{
    SECTION("ElevationEnergySeed with explicit per-band scale")
    {
        const json effectJson = json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisTile",
            "parameters": {
                "stat": "energy",
                "amount_source": "ElevationEnergySeed",
                "amount": 2,
                "op": "Add"
            }
        })");

        const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
        const auto* pMod = std::get_if<StatModifierEffect_t>(&config.effect);
        REQUIRE(pMod != nullptr);
        REQUIRE(pMod->amountSource.has_value());
        CHECK(*pMod->amountSource == StatModifierEffect_t::AmountSource_t::ElevationEnergySeed);
        CHECK(pMod->amount == Approx(2.0));
    }

    SECTION("amount defaults to 1 when amount_source is set")
    {
        const json effectJson = json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisTile",
            "parameters": { "stat": "energy", "amount_source": "ElevationEnergySeed" }
        })");

        const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
        const auto* pMod = std::get_if<StatModifierEffect_t>(&config.effect);
        REQUIRE(pMod != nullptr);
        CHECK(pMod->amount == Approx(1.0));
    }

    SECTION("amount_source on non-energy stat throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisTile",
            "parameters": { "stat": "minerals", "amount_source": "ElevationEnergySeed" }
        })")));
    }

    SECTION("amount_source outside ThisTile throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisBase",
            "parameters": { "stat": "energy", "amount_source": "ElevationEnergySeed" }
        })")));
    }

    SECTION("unknown amount_source throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisTile",
            "parameters": { "stat": "energy", "amount_source": "MoonPhase" }
        })")));
    }

    SECTION("amount_source with omitted op (defaults to Add) is OK")
    {
        CHECK_NOTHROW(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisTile",
            "parameters": { "stat": "energy", "amount_source": "ElevationEnergySeed" }
        })")));
    }

    SECTION("amount_source with explicit non-Add op throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisTile",
            "parameters": {
                "stat": "energy",
                "amount_source": "ElevationEnergySeed",
                "op": "MultiplyGeometric"
            }
        })")));
    }
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

        const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
        const auto* pMod = std::get_if<StatModifierEffect_t>(&config.effect);
        REQUIRE(pMod != nullptr);
        REQUIRE(pMod->selector.has_value());
        const auto* pHas = std::get_if<TileSelectorHasImprovement_t>(&*pMod->selector);
        REQUIRE(pHas);
        CHECK(pHas->improvement == "Farm");
    }

    SECTION("BaseTile selector (also the default kind)")
    {
        const json effectJson = json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisBase",
            "parameters": { "stat": "energy", "amount": 2, "selector": { "kind": "BaseTile" } }
        })");

        const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
        const auto* pMod = std::get_if<StatModifierEffect_t>(&config.effect);
        REQUIRE(pMod != nullptr);
        REQUIRE(pMod->selector.has_value());
        CHECK(std::holds_alternative<TileSelectorBaseTile_t>(*pMod->selector));
    }

    SECTION("AnyTile selector")
    {
        const json effectJson = json::parse(R"({
            "type": "StatModifier",
            "scope": "AllOwnerBases",
            "parameters": { "stat": "energy", "amount": 1, "selector": { "kind": "AnyTile" } }
        })");

        const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
        const auto* pMod = std::get_if<StatModifierEffect_t>(&config.effect);
        REQUIRE(pMod != nullptr);
        REQUIRE(pMod->selector.has_value());
        CHECK(std::holds_alternative<TileSelectorAnyTile_t>(*pMod->selector));
    }

    SECTION("HasImprovement without an improvement id throws")
    {
        const json selectorJson = json::parse(R"({ "kind": "HasImprovement" })");
        CHECK_THROWS(EffectConfigParser::ParseTileSelector(selectorJson));
    }

    SECTION("unknown selector kind throws")
    {
        const json selectorJson = json::parse(R"({ "kind": "Everything" })");
        CHECK_THROWS(EffectConfigParser::ParseTileSelector(selectorJson));
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
            CHECK_THROWS(EffectConfigParser::ParseEffectConfig(effectJson));
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

        const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
        REQUIRE(config.condition.has_value());
        const auto* pHas = std::get_if<TargetTileHas_t>(&config.condition->AsVariant());
        REQUIRE(pHas);
        CHECK(pHas->featureId == "Forest");
    }

    SECTION("empty condition value throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseCondition(json::parse(R"({ "kind": "TargetTileHas" })")));
    }

    SECTION("AllOf merges desugared values with nested conditions")
    {
        // Both arms in one node: each "values" entry becomes a TargetTileHas alternative and
        // the explicit "conditions" are appended, so only nested Condition_t nodes survive.
        const Condition_t condition = EffectConfigParser::ParseCondition(json::parse(R"({
            "kind": "AllOf",
            "values": ["Rocky"],
            "conditions": [{ "kind": "IsDefending" }]
        })"));

        const auto* pAllOf = std::get_if<AllOf_t>(&condition.AsVariant());
        REQUIRE(pAllOf);
        REQUIRE(pAllOf->conditions.size() == 2);
        const auto* pRocky = std::get_if<TargetTileHas_t>(&pAllOf->conditions[0].AsVariant());
        REQUIRE(pRocky);
        CHECK(pRocky->featureId == "Rocky");
        CHECK(std::holds_alternative<IsDefending_t>(pAllOf->conditions[1].AsVariant()));
    }

    SECTION("AllOf with neither values nor conditions throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseCondition(json::parse(R"({ "kind": "AllOf" })")));
        CHECK_THROWS(EffectConfigParser::ParseCondition(
            json::parse(R"({ "kind": "AllOf", "values": [] })")));
    }

    SECTION("unknown condition kind throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseCondition(
            json::parse(R"({ "kind": "TargetIsShiny", "value": "x" })")));
    }

    SECTION("IsHeadquarters condition")
    {
        const Condition_t condition = EffectConfigParser::ParseCondition(
            json::parse(R"({ "kind": "IsHeadquarters" })"));
        CHECK(std::holds_alternative<IsHeadquarters_t>(condition.AsVariant()));
    }
}

TEST_CASE("ParseEffectConfig: TransportParams", "[effects][parser][transport]")
{
    const json effectJson = json::parse(R"({
        "type": "TransportParams",
        "scope": "ThisUnit",
        "unitFilter": { "kind": "Domain", "domain": "sea" },
        "parameters": { "passenger_domains": ["air"] }
    })");

    const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
    const auto* pParams = std::get_if<TransportParamsEffect_t>(&config.effect);
    REQUIRE(pParams);
    REQUIRE(pParams->passengerDomains.size() == 1);
    CHECK(pParams->passengerDomains.front() == UnitDomain_t::Air);
    REQUIRE(config.unitFilter.has_value());
    const auto* pDomain = std::get_if<UnitFilterDomain_t>(&*config.unitFilter);
    REQUIRE(pDomain);
    CHECK(pDomain->domain == UnitDomain_t::Sea);

    const json loadSitesJson = json::parse(R"({
        "type": "TransportParams",
        "scope": "ThisUnit",
        "unitFilter": { "kind": "Domain", "domain": "air" },
        "parameters": {
            "load_site_flags": ["loads_air_transport", "refuels_air"]
        }
    })");
    const EffectConfig_t loadSites = EffectConfigParser::ParseEffectConfig(loadSitesJson);
    const auto* pLoad = std::get_if<TransportParamsEffect_t>(&loadSites.effect);
    REQUIRE(pLoad);
    CHECK(pLoad->passengerDomains.empty());
    REQUIRE(pLoad->loadSiteFlags.size() == 2);
    CHECK(pLoad->loadSiteFlags[0] == RuleFlagId_t::LoadsAirTransport);
    CHECK(pLoad->loadSiteFlags[1] == RuleFlagId_t::RefuelsAir);

    // Load sites name capabilities, not improvement or component ids.
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "TransportParams",
        "scope": "ThisUnit",
        "parameters": { "load_site_flags": ["Airbase"] }
    })")));

    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "TransportParams",
        "scope": "ThisBase",
        "parameters": { "passenger_domains": ["land"] }
    })")));
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "TransportParams",
        "scope": "ThisUnit",
        "parameters": {}
    })")));
}

TEST_CASE("ParseEffectConfig: unitFilter", "[effects][parser][unitFilter]")
{
    SECTION("Domain filter")
    {
        const json effectJson = json::parse(R"({
            "type": "StatModifier",
            "scope": "FactionUnits",
            "unitFilter": { "kind": "Domain", "domain": "air" },
            "parameters": { "stat": "starting_experience", "amount": 2 }
        })");

        const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
        REQUIRE(config.unitFilter.has_value());
        const auto* pDomain = std::get_if<UnitFilterDomain_t>(&*config.unitFilter);
        REQUIRE(pDomain);
        CHECK(pDomain->domain == UnitDomain_t::Air);
    }

    SECTION("HasComponent filter")
    {
        const json effectJson = json::parse(R"({
            "type": "RuleFlag",
            "scope": "FactionUnits",
            "unitFilter": { "kind": "HasComponent", "component": "test_weapon" },
            "parameters": { "flag": "forces_psi_combat" }
        })");

        const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
        REQUIRE(config.unitFilter.has_value());
        const auto* pComp = std::get_if<UnitFilterHasComponent_t>(&*config.unitFilter);
        REQUIRE(pComp);
        CHECK(pComp->component == "test_weapon");
    }

    SECTION("Domain without domain throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseUnitFilter(json::parse(R"({ "kind": "Domain" })")));
    }

    SECTION("HasComponent without component throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseUnitFilter(json::parse(R"({ "kind": "HasComponent" })")));
    }

    SECTION("unknown unitFilter kind throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseUnitFilter(json::parse(R"({ "kind": "Everything" })")));
    }

    SECTION("orbital domain parses")
    {
        CHECK(EffectConfigParser::ParseUnitDomain("orbital") == UnitDomain_t::Orbital);
    }

    SECTION("unknown domain throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseUnitDomain("space"));
    }
}

TEST_CASE("ParseEffectConfig: buildingFilter", "[effects][parser][buildingFilter]")
{
    SECTION("All filter")
    {
        const json effectJson = json::parse(R"({
            "type": "StatModifier",
            "scope": "FactionGlobal",
            "buildingFilter": { "kind": "All" },
            "parameters": { "stat": "facility_energy_upkeep", "amount": -50, "op": "AddPercent" }
        })");
        const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
        REQUIRE(config.buildingFilter.has_value());
        CHECK(std::holds_alternative<BuildingFilterAll_t>(*config.buildingFilter));
    }

    SECTION("BuildingId filter")
    {
        const json effectJson = json::parse(R"({
            "type": "StatModifier",
            "scope": "FactionGlobal",
            "buildingFilter": { "kind": "BuildingId", "building": "Recycling_Tanks" },
            "parameters": { "stat": "facility_energy_upkeep", "amount": -50, "op": "AddPercent" }
        })");
        const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
        REQUIRE(config.buildingFilter.has_value());
        const auto* pId = std::get_if<BuildingFilterId_t>(&*config.buildingFilter);
        REQUIRE(pId);
        CHECK(pId->buildingId == "Recycling_Tanks");
    }

    SECTION("Category filter")
    {
        const json effectJson = json::parse(R"({
            "type": "StatModifier",
            "scope": "FactionGlobal",
            "buildingFilter": { "kind": "Category", "category": "grow" },
            "parameters": { "stat": "facility_energy_upkeep", "amount": -25, "op": "AddPercent" }
        })");
        const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
        REQUIRE(config.buildingFilter.has_value());
        const auto* pCat = std::get_if<BuildingFilterCategory_t>(&*config.buildingFilter);
        REQUIRE(pCat);
        CHECK(pCat->category == GameCategory_t::Grow);
    }

    SECTION("unknown buildingFilter kind throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseBuildingFilter(
            json::parse(R"({ "kind": "Everything" })")));
    }

    SECTION("BuildingId requires building id")
    {
        CHECK_THROWS(EffectConfigParser::ParseBuildingFilter(
            json::parse(R"({ "kind": "BuildingId" })")));
    }
}

TEST_CASE("ParseEffectConfig: grant effects require their id parameter", "[effects][parser]")
{
    const json grantBuilding = json::parse(R"({
        "type": "GrantBuilding", "scope": "ThisBase",
        "parameters": { "building_id": "network_node" }
    })");
    const EffectConfig_t grantConfig = EffectConfigParser::ParseEffectConfig(grantBuilding);
    const auto* pGrant = std::get_if<GrantBuildingEffect_t>(&grantConfig.effect);
    REQUIRE(pGrant != nullptr);
    CHECK(pGrant->buildingId == "network_node");

    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(
        json::parse(R"({ "type": "GrantBuilding", "scope": "ThisBase", "parameters": {} })")));
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(
        json::parse(R"({ "type": "GrantTech", "scope": "FactionGlobal", "parameters": {} })")));
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(
        json::parse(R"({ "type": "GrantUnit", "scope": "FactionGlobal", "parameters": {} })")));

    const json grantTech = json::parse(R"({
        "type": "GrantTech", "scope": "FactionGlobal",
        "persistence": "Instantaneous",
        "parameters": { "tech_id": "biogenetics" }
    })");
    const EffectConfig_t techConfig = EffectConfigParser::ParseEffectConfig(grantTech);
    CHECK(techConfig.persistence == EffectPersistence_t::Instantaneous);
    const auto* pTech = std::get_if<GrantTechEffect_t>(&techConfig.effect);
    REQUIRE(pTech != nullptr);
    CHECK(pTech->techId == "biogenetics");
}

TEST_CASE("ParseEffectConfig: RuleFlag requires a valid flag", "[effects][parser]")
{
    const json flagJson = json::parse(R"({
        "type": "RuleFlag", "scope": "ThisUnit", "parameters": { "flag": "forces_psi_combat" }
    })");
    const EffectConfig_t flagConfig = EffectConfigParser::ParseEffectConfig(flagJson);
    const auto* pFlag = std::get_if<RuleFlagEffect_t>(&flagConfig.effect);
    REQUIRE(pFlag != nullptr);
    CHECK(pFlag->flag == RuleFlagId_t::ForcesPsiCombat);

    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(
        json::parse(R"({ "type": "RuleFlag", "scope": "ThisUnit", "parameters": {} })")));
}

TEST_CASE("ParseEffectConfig: Permission and AttackerIsEmbarked", "[effects][parser][permission]")
{
    const json enterJson = json::parse(R"({
        "type": "Permission", "scope": "ThisUnit",
        "parameters": { "permission": "Enter" },
        "condition": { "kind": "AllOf", "values": ["Water", "Base"] }
    })");
    const EffectConfig_t enterConfig = EffectConfigParser::ParseEffectConfig(enterJson);
    const auto* pEnter = std::get_if<PermissionEffect_t>(&enterConfig.effect);
    REQUIRE(pEnter != nullptr);
    CHECK(pEnter->permission == PermissionId_t::Enter);
    REQUIRE(enterConfig.condition.has_value());
    const auto* pAllOf = std::get_if<AllOf_t>(&enterConfig.condition->AsVariant());
    REQUIRE(pAllOf);
    REQUIRE(pAllOf->conditions.size() == 2);
    const auto* pWater = std::get_if<TargetTileHas_t>(&pAllOf->conditions[0].AsVariant());
    const auto* pBase = std::get_if<TargetTileHas_t>(&pAllOf->conditions[1].AsVariant());
    REQUIRE(pWater);
    REQUIRE(pBase);
    CHECK(pWater->featureId == "Water");
    CHECK(pBase->featureId == "Base");

    const json attackJson = json::parse(R"({
        "type": "Permission", "scope": "ThisUnit",
        "parameters": { "permission": "Attack" }
    })");
    const EffectConfig_t attackConfig = EffectConfigParser::ParseEffectConfig(attackJson);
    const auto* pAttack = std::get_if<PermissionEffect_t>(&attackConfig.effect);
    REQUIRE(pAttack != nullptr);
    CHECK(pAttack->permission == PermissionId_t::Attack);
    CHECK_FALSE(attackConfig.condition.has_value());

    const json embarkedJson = json::parse(R"({
        "type": "Permission", "scope": "ThisUnit",
        "parameters": { "permission": "Attack" },
        "condition": { "kind": "AttackerIsEmbarked" }
    })");
    const EffectConfig_t embarkedConfig = EffectConfigParser::ParseEffectConfig(embarkedJson);
    REQUIRE(embarkedConfig.condition.has_value());
    CHECK(std::holds_alternative<AttackerIsEmbarked_t>(embarkedConfig.condition->AsVariant()));

    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(
        json::parse(R"({ "type": "Permission", "scope": "ThisUnit", "parameters": {} })")));
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "Permission", "scope": "ThisUnit",
        "parameters": { "permission": "Fly" }
    })")));
}

TEST_CASE("ParseEffectConfig: TileResourceCap and apply_after_restriction", "[effects][parser]")
{
    const json capJson = json::parse(R"({
        "type": "TileResourceCap", "scope": "FactionGlobal",
        "removed_by_tech": "gene_splicing",
        "parameters": { "stat": "nutrients", "max": 2 }
    })");
    const EffectConfig_t capConfig = EffectConfigParser::ParseEffectConfig(capJson);
    const auto* pCap = std::get_if<TileResourceCapEffect_t>(&capConfig.effect);
    REQUIRE(pCap != nullptr);
    CHECK(pCap->stat == StatId_t::Nutrients);
    CHECK(pCap->max == 2);
    CHECK(capConfig.removedByTech == "gene_splicing");

    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "TileResourceCap", "scope": "ThisTile",
        "parameters": { "stat": "nutrients", "max": 2 }
    })")));

    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "TileResourceCap", "scope": "FactionGlobal",
        "parameters": { "stat": "nutrients" }
    })")));

    const json afterJson = json::parse(R"({
        "type": "StatModifier", "scope": "ThisTile",
        "parameters": { "stat": "nutrients", "amount": 2, "op": "Add", "apply_after_restriction": true }
    })");
    const EffectConfig_t afterConfig = EffectConfigParser::ParseEffectConfig(afterJson);
    const auto* pMod = std::get_if<StatModifierEffect_t>(&afterConfig.effect);
    REQUIRE(pMod != nullptr);
    CHECK(pMod->applyAfterRestriction);

    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "StatModifier", "scope": "ThisTile",
        "parameters": {
            "stat": "nutrients", "amount": 2, "op": "AddPercent",
            "apply_after_restriction": true
        }
    })")));
}

TEST_CASE("ParseEffectConfig: SocialRatingModifier", "[effects][parser]")
{
    const json ratingJson = json::parse(R"({
        "type": "SocialRatingModifier", "scope": "FactionGlobal",
        "parameters": { "rating": "police", "amount": -2 }
    })");
    const EffectConfig_t ratingConfig = EffectConfigParser::ParseEffectConfig(ratingJson);
    const auto* pRating = std::get_if<SocialRatingModifierEffect_t>(&ratingConfig.effect);
    REQUIRE(pRating != nullptr);
    CHECK(pRating->rating == SocialRatingId_t::Police);
    CHECK(pRating->amount == -2);

    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(
        json::parse(R"({ "type": "SocialRatingModifier", "scope": "FactionGlobal", "parameters": {} })")));
}

TEST_CASE("ParseEffectConfig: Infiltration uses scope + factionFilter", "[effects][parser]")
{
    const EffectConfig_t council = EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "Infiltration",
        "scope": "FactionGlobal",
        "persistence": "Continuous",
        "factionFilter": { "kind": "CouncilMembers" }
    })"));
    CHECK(council.scope == EffectScope_t::FactionGlobal);
    CHECK(council.persistence == EffectPersistence_t::Continuous);
    CHECK(std::get_if<InfiltrationEffect_t>(&council.effect));
    REQUIRE(council.factionFilter);
    CHECK(council.factionFilter->kind == FactionFilterKind_t::CouncilMembers);

    const EffectConfig_t world = EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "Infiltration",
        "scope": "WorldGlobal",
        "persistence": "Continuous"
    })"));
    CHECK(world.scope == EffectScope_t::WorldGlobal);
    CHECK_FALSE(world.factionFilter.has_value());

    const EffectConfig_t probe = EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "Infiltration",
        "scope": "FactionGlobal",
        "persistence": "Instantaneous",
        "factionFilter": { "kind": "ActionTarget" }
    })"));
    REQUIRE(probe.factionFilter);
    CHECK(probe.factionFilter->kind == FactionFilterKind_t::ActionTarget);

    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "Infiltration", "scope": "FactionGlobal", "persistence": "Continuous"
    })")));
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "Infiltration",
        "scope": "FactionGlobal",
        "persistence": "Continuous",
        "factionFilter": { "kind": "ActionTarget" }
    })")));
}

TEST_CASE("ParseEffectConfig: Conceal and Detect require a channel", "[effects][parser][detection]")
{
    const EffectConfig_t conceal = EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "Conceal", "scope": "ThisUnit", "parameters": { "channel": "cloak" }
    })"));
    const auto* pConceal = std::get_if<ConcealEffect_t>(&conceal.effect);
    REQUIRE(pConceal != nullptr);
    CHECK(pConceal->channel == "cloak");

    const EffectConfig_t detect = EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "Detect", "scope": "ThisTile", "radius": 2,
        "parameters": { "channel": "terrain" }
    })"));
    const auto* pDetect = std::get_if<DetectEffect_t>(&detect.effect);
    REQUIRE(pDetect != nullptr);
    CHECK(pDetect->channel == "terrain");
    CHECK(detect.radius == 2);

    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(
        json::parse(R"({ "type": "Conceal", "scope": "ThisUnit", "parameters": {} })")));
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(
        json::parse(R"({ "type": "Detect", "scope": "ThisTile", "parameters": {} })")));
}

TEST_CASE("ParseEffectConfig: unknown effect type throws", "[effects][parser]")
{
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(
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
        CHECK(EffectConfigParser::ParseEffectConfig(withRadius).radius == 2);

        const json withoutRadius = json::parse(R"({
            "type": "StatModifier", "scope": "ThisTile",
            "parameters": { "stat": "energy", "amount": 1 }
        })");
        CHECK(EffectConfigParser::ParseEffectConfig(withoutRadius).radius == 0);
    }

    SECTION("negative radius throws")
    {
        const json negative = json::parse(R"({
            "type": "StatModifier", "scope": "ThisTile", "radius": -1,
            "parameters": { "stat": "energy", "amount": 1 }
        })");
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(negative));
    }

    SECTION("nonzero radius on non-ThisTile throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier", "scope": "ThisBase", "radius": 2,
            "parameters": { "stat": "energy", "amount": 1 }
        })")));
    }
}

TEST_CASE("ParseEffectConfig: missing type or scope throws", "[effects][parser]")
{
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "scope": "ThisBase",
        "parameters": { "stat": "nutrients", "amount": 1 }
    })")));
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "StatModifier",
        "parameters": { "stat": "nutrients", "amount": 1 }
    })")));
}

TEST_CASE("ParseEffectConfig: required balance keys", "[effects][parser][orbital]")
{
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "OrbitalAttack", "scope": "FactionGlobal",
        "parameters": { "cooldown_turns": 1 }
    })")));
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "OrbitalAttack", "scope": "FactionGlobal",
        "parameters": { "chance": 50 }
    })")));
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "InterceptAttempt", "scope": "FactionGlobal",
        "parameters": {},
        "unitFilter": { "kind": "Domain", "domain": "orbital" }
    })")));

    const EffectConfig_t interceptNoCooldown = EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "InterceptAttempt", "scope": "FactionGlobal",
        "parameters": { "chance": 50 },
        "unitFilter": { "kind": "Domain", "domain": "orbital" }
    })"));
    const auto* pIntercept = std::get_if<InterceptAttemptEffect_t>(&interceptNoCooldown.effect);
    REQUIRE(pIntercept);
    CHECK(pIntercept->chance == 50);
    CHECK(pIntercept->cooldownTurns == -1);
}

TEST_CASE("ParseEffects: non-array effects throws", "[effects][parser]")
{
    CHECK_THROWS(EffectConfigParser::ParseEffects(json::parse(R"({
        "id": "bad",
        "effects": { "type": "StatModifier", "scope": "ThisBase" }
    })")));
}

TEST_CASE("ValidateScopeForSource: rejects only the certainly-impossible combinations",
          "[effects][parser][validation]")
{
    // ThisPop can only ever resolve against a pop type; ThisUnit against a unit component.
    CHECK_THROWS(EffectConfigParser::ValidateScopeForSource(
        EffectScope_t::ThisPop, EffectSourceKind_t::Building, "some_building"));
    CHECK_THROWS(EffectConfigParser::ValidateScopeForSource(
        EffectScope_t::ThisUnit, EffectSourceKind_t::PopType, "some_pop"));

    CHECK_NOTHROW(EffectConfigParser::ValidateScopeForSource(
        EffectScope_t::ThisPop, EffectSourceKind_t::PopType, "some_pop"));
    CHECK_NOTHROW(EffectConfigParser::ValidateScopeForSource(
        EffectScope_t::ThisUnit, EffectSourceKind_t::UnitComponent, "some_component"));

    // ThisBase / ProducedAtThisBase need an origin base (or pop-merge path).
    CHECK_THROWS(EffectConfigParser::ValidateScopeForSource(
        EffectScope_t::ThisBase, EffectSourceKind_t::UnitComponent, "sensor_pod"));
    CHECK_THROWS(EffectConfigParser::ValidateScopeForSource(
        EffectScope_t::ProducedAtThisBase, EffectSourceKind_t::Improvement, "monolith"));
    CHECK_THROWS(EffectConfigParser::ValidateScopeForSource(
        EffectScope_t::ThisBase, EffectSourceKind_t::CouncilProposal, "trade_pact"));
    CHECK_THROWS(EffectConfigParser::ValidateScopeForSource(
        EffectScope_t::ThisBase, EffectSourceKind_t::TileYieldRules, "tile_yield_rules"));
    CHECK_NOTHROW(EffectConfigParser::ValidateScopeForSource(
        EffectScope_t::ThisBase, EffectSourceKind_t::Building, "recycling_tanks"));
    CHECK_NOTHROW(EffectConfigParser::ValidateScopeForSource(
        EffectScope_t::ProducedAtThisBase, EffectSourceKind_t::Building, "aerospace"));

    // Legal-but-inert: faction-lane on improvement (pending territory) still loads.
    CHECK_NOTHROW(EffectConfigParser::ValidateScopeForSource(
        EffectScope_t::FactionGlobal, EffectSourceKind_t::Improvement, "monolith"));
    CHECK_NOTHROW(EffectConfigParser::ValidateScopeForSource(
        EffectScope_t::ThisTile, EffectSourceKind_t::UnitComponent, "sensor_pod"));
    CHECK_NOTHROW(EffectConfigParser::ValidateScopeForSource(
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
    CHECK_THROWS(EffectConfigParser::ParseEffects(badContainer, EffectSourceKind_t::Building, "bad_building"));
    CHECK_NOTHROW(EffectConfigParser::ParseEffects(badContainer, EffectSourceKind_t::PopType, "fine_as_pop"));
}

TEST_CASE("ParseEffects: absent effects array yields empty vector; entries parse in order", "[effects][parser]")
{
    CHECK(EffectConfigParser::ParseEffects(json::parse(R"({ "id": "no_effects" })")).empty());

    const json container = json::parse(R"({
        "id": "two_effects",
        "effects": [
            { "type": "StatModifier", "scope": "ThisBase", "parameters": { "stat": "nutrients", "amount": 1 } },
            { "type": "RuleFlag", "scope": "ThisUnit", "parameters": { "flag": "single_use" } }
        ]
    })");

    const std::vector<EffectConfig_t> effects = EffectConfigParser::ParseEffects(container);
    REQUIRE(effects.size() == 2);
    CHECK(std::holds_alternative<StatModifierEffect_t>(effects[0].effect));
    CHECK(std::holds_alternative<RuleFlagEffect_t>(effects[1].effect));
}
