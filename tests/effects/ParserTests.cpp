// Tests for EffectConfigParser — the single shared implementation of the JSON `effects`
// array schema used by every config source (buildings, unit components, pop types,
// improvements, social policies).

#include "game/effects/EffectConfig.h"
#include "game/effects/EffectConfigParser.h"
#include "game/units/MovementConstants.h"
#include "game/effects/TriggeredEffect.h"
#include "game/effects/TriggeredEffectParser.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <variant>

using namespace ac;
using Catch::Approx;
using Catch::Matchers::ContainsSubstring;
using nlohmann::json;

TEST_CASE("ParseStatId: canonical string mappings", "[effects][parser]")
{
    CHECK(ParseStatId("nutrients") == StatId_t::Nutrients);
    CHECK(ParseStatId("minerals") == StatId_t::Minerals);
    CHECK(ParseStatId("energy") == StatId_t::Energy);
    CHECK(ParseStatId("energy_credits") == StatId_t::EnergyCredits);
    CHECK(ParseStatId("econ") == StatId_t::Econ);
    CHECK(ParseStatId("labs") == StatId_t::Labs);
    CHECK(ParseStatId("psych") == StatId_t::Psych);
    CHECK(ParseStatId("drones") == StatId_t::Drones);
    CHECK(ParseStatId("talents") == StatId_t::Talents);
    CHECK(ParseStatId("attack") == StatId_t::Attack);
    CHECK(ParseStatId("defense") == StatId_t::Defense);
    CHECK(ParseStatId("tile_defense") == StatId_t::TileDefense);
    CHECK(ParseStatId("movement") == StatId_t::Movement);
    CHECK(ParseStatId("vision") == StatId_t::Vision);
    CHECK(ParseStatId("hit_points") == StatId_t::HitPoints);
    CHECK(ParseStatId("psi_damage") == StatId_t::PsiDamage);
    CHECK(ParseStatId("collateral_damage") == StatId_t::CollateralDamage);
    CHECK(ParseStatId("collateral_susceptibility") == StatId_t::CollateralSusceptibility);
    CHECK(ParseStatId("planet_pearls") == StatId_t::PlanetPearls);
    CHECK(ParseStatId("disengage_chance") == StatId_t::DisengageChance);
    CHECK(ParseStatId("turns_of_fuel") == StatId_t::TurnsOfFuel);
    CHECK(ParseStatId("damage_from_out_of_fuel") == StatId_t::DamageFromOutOfFuel);
    CHECK(ParseStatId("cargo_capacity") == StatId_t::CargoCapacity);
    CHECK(ParseStatId("difficult_terrain_cost")
          == StatId_t::DifficultTerrainCost);
    CHECK(ParseStatId("move_cost") == StatId_t::MoveCost);
    CHECK(ParseStatId("mineral_upkeep") == StatId_t::MineralUpkeep);
    CHECK(ParseStatId("free_unit_support") == StatId_t::FreeUnitSupport);
    CHECK(ParseStatId("max_police") == StatId_t::MaxPolice);
    CHECK(ParseStatId("police_effectiveness") == StatId_t::PoliceEffectiveness);
    CHECK(ParseStatId("away_from_home_drones") == StatId_t::AwayFromHomeDrones);
    CHECK(ParseStatId("cost_multiplier") == StatId_t::CostMultiplier);
    CHECK(ParseStatId("prototype_surcharge_scale") == StatId_t::PrototypeSurchargeScale);
    CHECK(ParseStatId("retool_penalty_scale") == StatId_t::RetoolPenaltyScale);
    CHECK(ParseStatId("facility_energy_upkeep") == StatId_t::FacilityEnergyUpkeep);
    CHECK(ParseStatId("probe_action_cost") == StatId_t::ProbeActionCost);
    CHECK(ParseStatId("probe_defense") == StatId_t::ProbeDefense);
    CHECK(ParseStatId("probe_failure_scale") == StatId_t::ProbeFailureScale);
    CHECK(ParseStatId("probe_success_scale") == StatId_t::ProbeSuccessScale);
    CHECK(ParseStatId("starting_minerals") == StatId_t::StartingMinerals);
    CHECK(ParseStatId("morale_bonus") == StatId_t::MoraleBonus);
    CHECK(ParseStatId("positive_morale_scale") == StatId_t::PositiveMoraleScale);
    CHECK(ParseStatId("growth_rate") == StatId_t::GrowthRate);
    CHECK(ParseStatId("starting_size") == StatId_t::StartingSize);
    CHECK(ParseStatId("max_base_size") == StatId_t::MaxBaseSize);
    CHECK(ParseStatId("tech_cost") == StatId_t::TechCost);
    CHECK(ParseStatId("moisture_tier") == StatId_t::MoistureTier);
    CHECK(ParseStatId("commerce_rate") == StatId_t::CommerceRate);
    CHECK(ParseStatId("commerce_rating") == StatId_t::CommerceRating);
    CHECK(ParseStatId("council_votes") == StatId_t::CouncilVotes);
    CHECK(ParseStatId("commerce_energy_bonus") == StatId_t::CommerceEnergyBonus);
    CHECK(ParseStatId("inefficiency_denominator") == StatId_t::InefficiencyDenominator);
    CHECK(ParseStatId("scrap_refund") == StatId_t::ScrapRefund);
    CHECK(ParseStatId("last_defender_pop_loss") == StatId_t::LastDefenderPopLoss);
    CHECK(ParseStatId("capture_pop_loss") == StatId_t::CapturePopLoss);
    CHECK(ParseStatId("conquered_drone_cap") == StatId_t::ConqueredDroneCap);
    CHECK(ParseStatId("ecological_damage") == StatId_t::EcologicalDamage);
    CHECK(ParseStatId("tech_cost_diff") == StatId_t::TechCostDiff);
    CHECK(ParseStatId("bureaucracy") == StatId_t::Bureaucracy);

    CHECK_THROWS(ParseStatId("not_a_stat"));
    CHECK_THROWS(ParseStatId(""));
    // Ids are case-sensitive.
    CHECK_THROWS(ParseStatId("Nutrients"));
}

TEST_CASE("ParseModifierOp / ParseEffectScope mappings", "[effects][parser]")
{
    CHECK(EffectConfigParser::ParseModifierOp("Add") == ModifierOp_t::Add);
    CHECK(EffectConfigParser::ParseModifierOp("AddPercent") == ModifierOp_t::AddPercent);
    CHECK(EffectConfigParser::ParseModifierOp("MultiplyGeometric") == ModifierOp_t::MultiplyGeometric);
    CHECK(EffectConfigParser::ParseModifierOp("MaxClamp") == ModifierOp_t::MaxClamp);
    CHECK(EffectConfigParser::ParseModifierOp("MinClamp") == ModifierOp_t::MinClamp);
    CHECK_THROWS(EffectConfigParser::ParseModifierOp("Multiply"));
    CHECK_THROWS(EffectConfigParser::ParseModifierOp("Unlimited"));

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
    CHECK(EffectConfigParser::ParseEffectScope("ThisTech") == EffectScope_t::ThisTech);
    CHECK_THROWS(EffectConfigParser::ParseEffectScope("Global"));
}

TEST_CASE("ParseRuleFlagId and ParseSocialRatingId mappings", "[effects][parser]")
{
    CHECK(ParseRuleFlagId("single_use") == RuleFlagId_t::SingleUse);
    CHECK(ParseRuleFlagId("population_boom") == RuleFlagId_t::PopulationBoom);
    CHECK(ParseRuleFlagId("near_zero_growth") == RuleFlagId_t::NearZeroGrowth);
    CHECK(ParseRuleFlagId("ignores_difficult_terrain") == RuleFlagId_t::IgnoreDifficultTerrain);
    CHECK(ParseRuleFlagId("forces_psi_combat") == RuleFlagId_t::ForcesPsiCombat);
    CHECK(ParseRuleFlagId("native_life") == RuleFlagId_t::NativeLife);
    CHECK(ParseRuleFlagId("found_base") == RuleFlagId_t::FoundBase);
    CHECK(ParseRuleFlagId("terraform") == RuleFlagId_t::Terraform);
    CHECK(ParseRuleFlagId("supply_crawl") == RuleFlagId_t::SupplyCrawl);
    CHECK(ParseRuleFlagId("probe_team") == RuleFlagId_t::ProbeTeam);
    CHECK(ParseRuleFlagId("non_combatant") == RuleFlagId_t::NonCombatant);
    CHECK(ParseRuleFlagId("cannot_capture_bases")
          == RuleFlagId_t::CannotCaptureBases);
    CHECK(ParseRuleFlagId("attacking_ends_turn")
          == RuleFlagId_t::AttackingEndsTurn);
    CHECK(ParseRuleFlagId("no_conquest_repair")
          == RuleFlagId_t::NoConquestRepair);
    CHECK(ParseRuleFlagId("headquarters") == RuleFlagId_t::Headquarters);
    CHECK(ParseRuleFlagId("probe_subversion_immune")
          == RuleFlagId_t::ProbeSubversionImmune);
    CHECK(ParseRuleFlagId("blocks_probe_teams")
          == RuleFlagId_t::BlocksProbeTeams);
    CHECK(ParseRuleFlagId("ignores_probe_block")
          == RuleFlagId_t::IgnoresProbeBlock);
    CHECK(ParseRuleFlagId("airdrop_launch") == RuleFlagId_t::AirdropLaunch);
    CHECK(ParseRuleFlagId("airdrop_interdiction") == RuleFlagId_t::AirdropInterdiction);
    CHECK(ParseRuleFlagId("creche") == RuleFlagId_t::Creche);
    CHECK(ParseRuleFlagId("prevents_disengage") == RuleFlagId_t::PreventsDisengage);
    CHECK(ParseRuleFlagId("harbors") == RuleFlagId_t::Harbors);
    CHECK(ParseRuleFlagId("remove_shroud") == RuleFlagId_t::RemoveShroud);
    CHECK(ParseRuleFlagId("remove_fog") == RuleFlagId_t::RemoveFog);
    CHECK(ParseRuleFlagId("visible_in_fog") == RuleFlagId_t::VisibleInFog);
    CHECK(ParseRuleFlagId("non_combatants_destroyed_without_combatant")
          == RuleFlagId_t::NonCombatantsDestroyedWithoutCombatant);
    CHECK(ParseRuleFlagId("atrocities_forbidden")
          == RuleFlagId_t::AtrocitiesForbidden);
    CHECK(ParseRuleFlagId("disable_production")
          == RuleFlagId_t::DisableProduction);
    CHECK_THROWS(ParseRuleFlagId("hover"));
    CHECK_THROWS(ParseRuleFlagId("sea"));
    CHECK_THROWS(ParseRuleFlagId("prevents_conquest_pop_loss"));

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
        "parameters": { "stat": "minerals", "amount": 3, "op": "AddPercent" }
    })");

    const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
    CHECK(config.scope == EffectScope_t::ThisBase);
    CHECK_FALSE(config.condition.has_value());

    const auto* pMod = std::get_if<StatModifierEffect_t>(&config.effect);
    REQUIRE(pMod != nullptr);
    CHECK(pMod->stat == StatId_t::Minerals);
    CHECK(pMod->amount == Approx(3.0));
    CHECK(pMod->op == ModifierOp_t::AddPercent);
    CHECK_FALSE(pMod->selector.has_value());
}

TEST_CASE("ParseEffectConfig: defaults — op Add, amount 0", "[effects][parser]")
{
    const json effectJson = json::parse(R"({
        "type": "StatModifier",
        "scope": "ThisTile",
        "parameters": { "stat": "energy" }
    })");

    const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);

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
    SECTION("ElevationEnergy with explicit per-band scale")
    {
        const json effectJson = json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisTile",
            "parameters": {
                "stat": "energy",
                "amount_source": "ElevationEnergy",
                "amount": 2,
                "op": "Add"
            }
        })");

        const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
        const auto* pMod = std::get_if<StatModifierEffect_t>(&config.effect);
        REQUIRE(pMod != nullptr);
        REQUIRE(pMod->amountSource.has_value());
        CHECK(*pMod->amountSource == StatModifierEffect_t::AmountSource_t::ElevationEnergy);
        CHECK(pMod->amount == Approx(2.0));
    }

    SECTION("amount defaults to 1 when amount_source is set")
    {
        const json effectJson = json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisTile",
            "parameters": { "stat": "energy", "amount_source": "ElevationEnergy" }
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
            "parameters": { "stat": "minerals", "amount_source": "ElevationEnergy" }
        })")));
    }

    SECTION("amount_source outside ThisTile throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisBase",
            "parameters": { "stat": "energy", "amount_source": "ElevationEnergy" }
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
            "parameters": { "stat": "energy", "amount_source": "ElevationEnergy" }
        })")));
    }

    SECTION("amount_source works with any op, because it computes the amount not the operation")
    {
        // "Cap drone pressure at base size" — pop_composition.json ships exactly this. The op
        // used to be restricted to Add, which was a parse-time guard rather than a limitation:
        // ResolveStatModifiers calls AmountSourceValue for every contribution and hands
        // (amount, op) to ApplyModifierStack, which handles clamps identically.
        const EffectConfig_t clamp = EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisBase",
            "parameters": {
                "stat": "drones",
                "amount": 1,
                "amount_source": "BaseSize",
                "op": "MaxClamp"
            }
        })"));
        const auto* pMod = std::get_if<StatModifierEffect_t>(&clamp.effect);
        REQUIRE(pMod != nullptr);
        CHECK(pMod->op == ModifierOp_t::MaxClamp);
        CHECK(pMod->amountSource == StatModifierEffect_t::AmountSource_t::BaseSize);

        CHECK_NOTHROW(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisTile",
            "parameters": {
                "stat": "energy",
                "amount_source": "ElevationEnergy",
                "op": "MultiplyGeometric"
            }
        })")));

        // Per-source legality still constrains it: BaseSize is Base-domain stats only.
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisUnit",
            "parameters": {
                "stat": "attack",
                "amount_source": "BaseSize",
                "op": "MaxClamp"
            }
        })")));
    }

    SECTION("MineralsConverted on econ ThisBase")
    {
        const json effectJson = json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisBase",
            "parameters": {
                "stat": "econ",
                "amount_source": "MineralsConverted",
                "amount": 0.5,
                "op": "Add"
            }
        })");

        const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
        const auto* pMod = std::get_if<StatModifierEffect_t>(&config.effect);
        REQUIRE(pMod != nullptr);
        REQUIRE(pMod->amountSource.has_value());
        CHECK(*pMod->amountSource == StatModifierEffect_t::AmountSource_t::MineralsConverted);
        CHECK(pMod->amount == Approx(0.5));
        CHECK(pMod->stat == StatId_t::Econ);
    }

    SECTION("MineralsConverted on nutrients is OK")
    {
        CHECK_NOTHROW(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisBase",
            "parameters": { "stat": "nutrients", "amount_source": "MineralsConverted", "amount": 1 }
        })")));
    }

    SECTION("MineralsConverted on minerals throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisBase",
            "parameters": { "stat": "minerals", "amount_source": "MineralsConverted" }
        })")));
    }

    // Energy is a legal output; conversion routes it through the slider split rather than
    // crediting a bank directly.
    SECTION("MineralsConverted on energy is OK")
    {
        CHECK_NOTHROW(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisBase",
            "parameters": { "stat": "energy", "amount_source": "MineralsConverted", "amount": 1 }
        })")));
    }

    SECTION("MineralsConverted outside ThisBase throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisTile",
            "parameters": { "stat": "econ", "amount_source": "MineralsConverted" }
        })")));
    }

    SECTION("MineralsConverted amount 0 throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisBase",
            "parameters": { "stat": "econ", "amount": 0, "amount_source": "MineralsConverted" }
        })")));
    }

    SECTION("MineralsConverted with a tile selector throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisBase",
            "parameters": {
                "stat": "econ",
                "amount_source": "MineralsConverted",
                "selector": { "kind": "BaseTile" }
            }
        })")));
    }

    SECTION("BaseSize University-style scale")
    {
        const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisBase",
            "parameters": {
                "stat": "drones",
                "amount_source": "BaseSize",
                "amount": 0.25,
                "op": "Add"
            }
        })"));
        const auto* pMod = std::get_if<StatModifierEffect_t>(&config.effect);
        REQUIRE(pMod != nullptr);
        REQUIRE(pMod->amountSource.has_value());
        CHECK(*pMod->amountSource == StatModifierEffect_t::AmountSource_t::BaseSize);
        CHECK(pMod->amount == Approx(0.25));
    }

    SECTION("BaseSize on AllOwnerBases is OK")
    {
        CHECK_NOTHROW(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "AllOwnerBases",
            "parameters": { "stat": "drones", "amount_source": "BaseSize", "amount": 0.25 }
        })")));
    }

    SECTION("BaseSize outside base-level scopes throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisTile",
            "parameters": { "stat": "drones", "amount_source": "BaseSize", "amount": 0.25 }
        })")));
    }

    SECTION("BaseSize with a tile selector throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisBase",
            "parameters": {
                "stat": "energy",
                "amount_source": "BaseSize",
                "amount": 1,
                "selector": { "kind": "BaseTile" }
            }
        })")));
    }

    SECTION("BaseSize on a Unit-domain stat throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisBase",
            "parameters": { "stat": "attack", "amount_source": "BaseSize", "amount": 0.25 }
        })")));
    }

    SECTION("BasesOwned Empire Pulse-style scale")
    {
        const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisUnit",
            "parameters": {
                "stat": "attack",
                "amount_source": "BasesOwned",
                "amount": 1,
                "op": "Add"
            }
        })"));
        const auto* pMod = std::get_if<StatModifierEffect_t>(&config.effect);
        REQUIRE(pMod != nullptr);
        REQUIRE(pMod->amountSource.has_value());
        CHECK(*pMod->amountSource == StatModifierEffect_t::AmountSource_t::BasesOwned);
        CHECK(pMod->amount == Approx(1.0));
    }

    SECTION("IntrinsicXp Isle-style cargo scale")
    {
        const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisUnit",
            "parameters": {
                "stat": "cargo_capacity",
                "amount_source": "IntrinsicXp",
                "amount": 1,
                "op": "Add"
            }
        })"));
        const auto* pMod = std::get_if<StatModifierEffect_t>(&config.effect);
        REQUIRE(pMod != nullptr);
        REQUIRE(pMod->amountSource.has_value());
        CHECK(*pMod->amountSource == StatModifierEffect_t::AmountSource_t::IntrinsicXp);
        CHECK(pMod->amount == Approx(1.0));
    }

    SECTION("IntrinsicXp non-Add op throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisUnit",
            "parameters": {
                "stat": "cargo_capacity",
                "amount_source": "IntrinsicXp",
                "amount": 1,
                "op": "AddPercent"
            }
        })")));
    }

    SECTION("BasesOwned outside ThisUnit throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisBase",
            "parameters": { "stat": "attack", "amount_source": "BasesOwned", "amount": 1 }
        })")));
    }

    SECTION("BasesOwned on a Base-domain stat throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisUnit",
            "parameters": { "stat": "drones", "amount_source": "BasesOwned", "amount": 1 }
        })")));
    }

    SECTION("BuildingUpkeep MaxClamp on econ")
    {
        const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisBase",
            "parameters": {
                "stat": "econ",
                "amount_source": "BuildingUpkeep",
                "amount": 1,
                "op": "MaxClamp"
            }
        })"));
        const auto* pMod = std::get_if<StatModifierEffect_t>(&config.effect);
        REQUIRE(pMod != nullptr);
        REQUIRE(pMod->amountSource.has_value());
        CHECK(*pMod->amountSource == StatModifierEffect_t::AmountSource_t::BuildingUpkeep);
        CHECK(pMod->op == ModifierOp_t::MaxClamp);
        CHECK(pMod->amount == Approx(1.0));
    }

    SECTION("BuildingUpkeep on non-econ throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisBase",
            "parameters": {
                "stat": "energy",
                "amount_source": "BuildingUpkeep",
                "op": "MaxClamp"
            }
        })")));
    }

    SECTION("BuildingUpkeep without MaxClamp throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisBase",
            "parameters": { "stat": "econ", "amount_source": "BuildingUpkeep", "op": "Add" }
        })")));
    }

    SECTION("ElevationEnergy with a radius throws")
    {
        // targetTile is the receiving tile, so an aura would scale off whatever tile it
        // landed on rather than its own host — silently wrong rather than merely useless.
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisTile",
            "radius": 1,
            "parameters": { "stat": "energy", "amount_source": "ElevationEnergy", "amount": 1 }
        })")));
    }

    SECTION("any amount_source with a tile selector throws")
    {
        // Selectors resolve during tile-yield, which supplies only a tile subject — so the
        // rejection is on the combination, not on a per-source list that the next source
        // added would quietly fall off.
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisBase",
            "parameters": {
                "stat": "nutrients",
                "amount_source": "BaseSize",
                "amount": 1,
                "selector": { "kind": "Improvement", "improvement": "Farm" }
            }
        })")));
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier",
            "scope": "ThisTile",
            "parameters": {
                "stat": "energy",
                "amount_source": "ElevationEnergy",
                "amount": 1,
                "selector": { "kind": "Improvement", "improvement": "Mine" }
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

    SECTION("HasAirdroppedThisTurn condition")
    {
        const Condition_t condition = EffectConfigParser::ParseCondition(
            json::parse(R"({ "kind": "HasAirdroppedThisTurn" })"));
        CHECK(std::holds_alternative<HasAirdroppedThisTurn_t>(condition.AsVariant()));
    }

    SECTION("OriginBaseIsHomeBase condition")
    {
        const Condition_t condition = EffectConfigParser::ParseCondition(
            json::parse(R"({ "kind": "OriginBaseIsHomeBase" })"));
        CHECK(std::holds_alternative<OriginBaseIsHomeBase_t>(condition.AsVariant()));
    }
}

TEST_CASE("ParseEffectConfig: TransportParams", "[effects][parser][transport]")
{
    const json effectJson = json::parse(R"({
        "type": "TransportParams",
        "scope": "ThisUnit",
        "condition": { "kind": "SubjectDomain", "domain": "sea" },
        "parameters": { "carries": ["air"] }
    })");

    const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
    const auto* pParams = std::get_if<TransportParamsEffect_t>(&config.effect);
    REQUIRE(pParams);
    REQUIRE(pParams->carries.size() == 1);
    CHECK(pParams->carries.front() == UnitDomain_t::Air);
    CHECK_FALSE(pParams->requiresHarbor);
    REQUIRE(config.condition.has_value());
    const auto* pDomain = std::get_if<SubjectDomain_t>(&*config.condition);
    REQUIRE(pDomain);
    CHECK(pDomain->domain == UnitDomain_t::Sea);

    const json harborOnlyJson = json::parse(R"({
        "type": "TransportParams",
        "scope": "ThisUnit",
        "condition": { "kind": "SubjectDomain", "domain": "air" },
        "parameters": { "requires_harbor": true }
    })");
    const EffectConfig_t harborOnly = EffectConfigParser::ParseEffectConfig(harborOnlyJson);
    const auto* pHarbor = std::get_if<TransportParamsEffect_t>(&harborOnly.effect);
    REQUIRE(pHarbor);
    CHECK(pHarbor->carries.empty());
    CHECK(pHarbor->requiresHarbor);

    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "TransportParams",
        "scope": "ThisUnit",
        "parameters": { "passenger_domains": ["air"] }
    })")));
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "TransportParams",
        "scope": "ThisUnit",
        "parameters": { "load_site_flags": ["loads_air_transport"] }
    })")));
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "TransportParams",
        "scope": "ThisUnit",
        "parameters": { "carries": ["air"], "refuels_cargo": true }
    })")));
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "TransportParams",
        "scope": "ThisUnit",
        "parameters": {}
    })")));

    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "TransportParams",
        "scope": "ThisBase",
        "parameters": { "carries": ["land"] }
    })")));
}

TEST_CASE("ParseEffectConfig: move_cost MaxClamp stores move fragments", "[effects][parser][movement]")
{
    const json effectJson = json::parse(R"({
        "type": "StatModifier",
        "scope": "ThisUnit",
        "condition": { "kind": "TargetTileHas", "value": "Fungus" },
        "parameters": { "stat": "move_cost", "amount": "1/3", "op": "MaxClamp" }
    })");

    const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
    const auto* pModifier = std::get_if<StatModifierEffect_t>(&config.effect);
    REQUIRE(pModifier);
    CHECK(pModifier->stat == StatId_t::MoveCost);
    CHECK(pModifier->op == ModifierOp_t::MaxClamp);
    CHECK(pModifier->amount == MovementConstants_t::k_moveFragmentsPerPoint / 3);

    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "StatModifier",
        "scope": "ThisTile",
        "parameters": { "stat": "move_cost", "amount": 1, "op": "Add" }
    })")));
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "StatModifier",
        "scope": "ThisTile",
        "parameters": { "stat": "move_cost", "op": "MaxClamp" }
    })")));
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "StatModifier",
        "scope": "ThisTile",
        "parameters": { "stat": "move_cost", "amount": "1/7", "op": "MaxClamp" }
    })")));
}

TEST_CASE("ParseEffectConfig: identity condition arms", "[effects][parser][condition]")
{
    SECTION("Domain")
    {
        const json effectJson = json::parse(R"({
            "type": "StatModifier",
            "scope": "FactionUnits",
            "condition": { "kind": "SubjectDomain", "domain": "air" },
            "parameters": { "stat": "attack", "amount": 2 }
        })");

        const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
        REQUIRE(config.condition.has_value());
        const auto* pDomain = std::get_if<SubjectDomain_t>(&*config.condition);
        REQUIRE(pDomain);
        CHECK(pDomain->domain == UnitDomain_t::Air);
    }

    SECTION("HasComponent")
    {
        const json effectJson = json::parse(R"({
            "type": "RuleFlag",
            "scope": "FactionUnits",
            "condition": { "kind": "HasComponent", "component": "test_weapon" },
            "parameters": { "flag": "forces_psi_combat" }
        })");

        const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
        REQUIRE(config.condition.has_value());
        const auto* pComp = std::get_if<HasComponent_t>(&*config.condition);
        REQUIRE(pComp);
        CHECK(pComp->component == "test_weapon");
    }

    SECTION("IsPrototype")
    {
        const json effectJson = json::parse(R"({
            "type": "StatModifier",
            "scope": "FactionUnits",
            "condition": { "kind": "IsPrototype" },
            "parameters": { "stat": "attack", "amount": 1 }
        })");

        const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
        REQUIRE(config.condition.has_value());
        CHECK(std::holds_alternative<IsPrototype_t>(*config.condition));
    }

    SECTION("IsCombatUnit")
    {
        const json effectJson = json::parse(R"({
            "type": "StatModifier",
            "scope": "FactionUnits",
            "condition": { "kind": "IsCombatUnit" },
            "parameters": { "stat": "police_effectiveness", "amount": 1 }
        })");

        const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
        REQUIRE(config.condition.has_value());
        CHECK(std::holds_alternative<IsCombatUnit_t>(*config.condition));
    }

    SECTION("IsNativeLife")
    {
        const json effectJson = json::parse(R"({
            "type": "StatModifier",
            "scope": "FactionUnits",
            "condition": { "kind": "IsNativeLife" },
            "parameters": { "stat": "attack", "amount": 1 }
        })");

        const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
        REQUIRE(config.condition.has_value());
        const auto* pNative = std::get_if<IsNativeLife_t>(&*config.condition);
        REQUIRE(pNative);
        CHECK(pNative->bMatches);

        const json negated = json::parse(R"({ "kind": "IsNativeLife", "value": false })");
        const Condition_t parsed = EffectConfigParser::ParseCondition(negated);
        const auto* pNegated = std::get_if<IsNativeLife_t>(&parsed);
        REQUIRE(pNegated);
        CHECK_FALSE(pNegated->bMatches);

        CHECK_THROWS(EffectConfigParser::ParseCondition(
            json::parse(R"({ "kind": "IsNativeLife", "value": "yes" })")));
    }

    SECTION("Domain without domain throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseCondition(json::parse(R"({ "kind": "SubjectDomain" })")));
    }

    SECTION("HasComponent without component throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseCondition(json::parse(R"({ "kind": "HasComponent" })")));
    }

    SECTION("SubjectDesign")
    {
        const json effectJson = json::parse(R"({
            "type": "RuleFlag",
            "scope": "FactionUnits",
            "condition": { "kind": "SubjectDesign", "design": "Alien_Artifact" },
            "parameters": { "flag": "forces_psi_combat" }
        })");

        const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
        REQUIRE(config.condition.has_value());
        const auto* pDesign = std::get_if<SubjectDesign_t>(&*config.condition);
        REQUIRE(pDesign);
        CHECK(pDesign->designId == "Alien_Artifact");
    }

    SECTION("SubjectDesign without design throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseCondition(json::parse(R"({ "kind": "SubjectDesign" })")));
    }

    SECTION("BaseHasBuilding")
    {
        const json effectJson = json::parse(R"({
            "type": "RuleFlag",
            "scope": "ThisUnit",
            "condition": { "kind": "BaseHasBuilding", "building": "Network_Node" },
            "parameters": { "flag": "forces_psi_combat" }
        })");

        const EffectConfig_t config = EffectConfigParser::ParseEffectConfig(effectJson);
        REQUIRE(config.condition.has_value());
        const auto* pBuilding = std::get_if<BaseHasBuilding_t>(&*config.condition);
        REQUIRE(pBuilding);
        CHECK(pBuilding->buildingId == "Network_Node");
    }

    SECTION("BaseHasBuilding without building throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseCondition(json::parse(R"({ "kind": "BaseHasBuilding" })")));
    }

    SECTION("unknown condition kind throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseCondition(json::parse(R"({ "kind": "Everything" })")));
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

TEST_CASE("ParseTriggeredEffectConfig: ModifyPopulation", "[effects][parser][triggered]")
{
    const json absJson = json::parse(R"({
        "type": "ModifyPopulation",
        "parameters": { "amount": -1 }
    })");
    const TriggeredEffectConfig_t absConfig =
        TriggeredEffectParser::ParseTriggeredEffectConfig(absJson, "on_complete_effects");
    const auto* pAbs = std::get_if<ModifyPopulationEffect_t>(&absConfig.effect);
    REQUIRE(pAbs != nullptr);
    CHECK(pAbs->amount == -1);
    CHECK(pAbs->op == ModifierOp_t::Add);
    CHECK(pAbs->minSize == 0);

    const json pctJson = json::parse(R"({
        "type": "ModifyPopulation",
        "parameters": { "amount": -50, "op": "AddPercent", "min_size": 1 }
    })");
    const TriggeredEffectConfig_t pctConfig =
        TriggeredEffectParser::ParseTriggeredEffectConfig(pctJson, "on_success_effects");
    const auto* pPct = std::get_if<ModifyPopulationEffect_t>(&pctConfig.effect);
    REQUIRE(pPct != nullptr);
    CHECK(pPct->amount == -50);
    CHECK(pPct->op == ModifierOp_t::AddPercent);
    CHECK(pPct->minSize == 1);

    CHECK_THROWS(TriggeredEffectParser::ParseTriggeredEffectConfig(
        json::parse(R"({ "type": "ModifyPopulation", "parameters": {} })"), "on_enter_effects"));
    CHECK_THROWS(TriggeredEffectParser::ParseTriggeredEffectConfig(
        json::parse(R"({
            "type": "ModifyPopulation",
            "parameters": { "amount": -1, "op": "MultiplyGeometric" }
        })"), "on_enter_effects"));
}

TEST_CASE("ParseTriggeredEffectConfig: DestroyFacility", "[effects][parser][triggered]")
{
    const TriggeredEffectConfig_t config = TriggeredEffectParser::ParseTriggeredEffectConfig(
        json::parse(R"({
            "type": "DestroyFacility",
            "parameters": { "count": 2, "exclude_hq": false, "exclude_secret_projects": true }
        })"), "on_enter_effects");
    const auto* pDestroy = std::get_if<DestroyFacilityEffect_t>(&config.effect);
    REQUIRE(pDestroy != nullptr);
    CHECK(pDestroy->count == 2);
    CHECK_FALSE(pDestroy->excludeHq);
    CHECK(pDestroy->excludeSecretProjects);

    // Every parameter is required: which facilities are off-limits is a per-caller rule, and
    // a silent default is what let the shipping config and this fixture disagree about
    // whether sabotage may destroy a secret project.
    CHECK_THROWS_WITH(TriggeredEffectParser::ParseTriggeredEffectConfig(
        json::parse(R"({ "type": "DestroyFacility", "parameters": {} })"), "on_enter_effects"),
        ContainsSubstring("count"));
    CHECK_THROWS_WITH(TriggeredEffectParser::ParseTriggeredEffectConfig(json::parse(R"({
        "type": "DestroyFacility",
        "parameters": { "count": 1, "exclude_secret_projects": true }
    })"), "on_enter_effects"), ContainsSubstring("exclude_hq"));
    CHECK_THROWS_WITH(TriggeredEffectParser::ParseTriggeredEffectConfig(json::parse(R"({
        "type": "DestroyFacility",
        "parameters": { "count": 1, "exclude_hq": true }
    })"), "on_enter_effects"), ContainsSubstring("exclude_secret_projects"));
    CHECK_THROWS_WITH(TriggeredEffectParser::ParseTriggeredEffectConfig(json::parse(R"({
        "type": "DestroyFacility",
        "parameters": { "count": 0, "exclude_hq": true, "exclude_secret_projects": true }
    })"), "on_enter_effects"), ContainsSubstring("count"));
}

TEST_CASE("ParseTriggeredEffectConfig: Rebel", "[effects][parser][triggered]")
{
    const TriggeredEffectConfig_t config = TriggeredEffectParser::ParseTriggeredEffectConfig(
        json::parse(R"({ "type": "Rebel" })"), "on_enter_effects");
    REQUIRE(std::get_if<RebelEffect_t>(&config.effect) != nullptr);
    CHECK_FALSE(config.oncePer.has_value());
}

TEST_CASE("ParseTriggeredEffectConfig: DestroyUnit", "[effects][parser][triggered]")
{
    const TriggeredEffectConfig_t config = TriggeredEffectParser::ParseTriggeredEffectConfig(
        json::parse(R"({ "type": "DestroyUnit" })"), "on_hold_effects");
    REQUIRE(std::get_if<DestroyUnitEffect_t>(&config.effect) != nullptr);
}

// The two families are separate types, so a mis-filed entry is a parse error that names the
// list it belongs in — where the old persistence flag let it parse and then never fire.
TEST_CASE("Effects and triggered effects reject each other's types", "[effects][parser][triggered]")
{
    CHECK_THROWS_WITH(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "Rebel", "scope": "ThisBase"
    })")), ContainsSubstring("on_enter_effects"));
    CHECK_THROWS_WITH(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "ModifyPopulation", "scope": "ThisBase", "parameters": { "amount": -1 }
    })")), ContainsSubstring("one-shot"));

    CHECK_THROWS_WITH(TriggeredEffectParser::ParseTriggeredEffectConfig(json::parse(R"({
        "type": "StatModifier", "parameters": { "stat": "minerals", "amount": 1 }
    })"), "on_enter_effects"), ContainsSubstring("belongs in 'effects'"));
    CHECK_THROWS_WITH(TriggeredEffectParser::ParseTriggeredEffectConfig(
        json::parse(R"({ "type": "NotAnEffect" })"), "on_enter_effects"),
        ContainsSubstring("Unknown triggered effect type"));
}

// Scope, persistence, radius and buildingFilter describe *where* a continuous effect resolves.
// A triggered effect gets timing from its list, so carrying those is a config error. `condition`
// is allowed (gates against subjects).
TEST_CASE("ParseTriggeredEffectConfig: rejects continuous-only keys", "[effects][parser][triggered]")
{
    for (const char* pKey : {"scope", "persistence", "radius", "min_radius",
                             "buildingFilter", "removed_by_tech"})
    {
        json effectJson = json::parse(R"({ "type": "Rebel" })");
        effectJson[pKey] = "ThisBase";
        CHECK_THROWS_WITH(
            TriggeredEffectParser::ParseTriggeredEffectConfig(effectJson, "on_enter_effects"),
            ContainsSubstring(pKey));
    }
}

TEST_CASE("ParseTriggeredEffectConfig: once_per", "[effects][parser][triggered]")
{
    const TriggeredEffectConfig_t config = TriggeredEffectParser::ParseTriggeredEffectConfig(
        json::parse(R"({
            "type": "GrantTech",
            "once_per": { "scope": "unit", "key": "monolith_xp" },
            "parameters": { "tech_id": "some_tech" }
        })"), "on_visit_effects");
    REQUIRE(config.oncePer.has_value());
    CHECK(config.oncePer->scope == OnceScope_t::Unit);
    CHECK(config.oncePer->key == "monolith_xp");

    // The key is what makes the rule span instances, so it cannot be omitted and defaulted.
    CHECK_THROWS_WITH(TriggeredEffectParser::ParseTriggeredEffectConfig(json::parse(R"({
        "type": "Rebel", "once_per": { "scope": "base" }
    })"), "on_enter_effects"), ContainsSubstring("key"));
    CHECK_THROWS_WITH(TriggeredEffectParser::ParseTriggeredEffectConfig(json::parse(R"({
        "type": "Rebel", "once_per": { "scope": "galaxy", "key": "k" }
    })"), "on_enter_effects"), ContainsSubstring("scope"));
    const TriggeredEffectConfig_t worldOnce = TriggeredEffectParser::ParseTriggeredEffectConfig(
        json::parse(R"({
            "type": "GrantTech",
            "once_per": { "scope": "world", "key": "first" },
            "parameters": { "selection": "Available" }
        })"), "on_discover_effects");
    REQUIRE(worldOnce.oncePer.has_value());
    CHECK(worldOnce.oncePer->scope == OnceScope_t::World);
    // Nor may scope be omitted: defaulting it to "unit" would turn a typo into an entry that
    // silently never fires from a trigger with no unit.
    CHECK_THROWS_WITH(TriggeredEffectParser::ParseTriggeredEffectConfig(json::parse(R"({
        "type": "Rebel", "once_per": { "key": "k" }
    })"), "on_enter_effects"), ContainsSubstring("scope"));
}

// Only SetInfiltration reads the filter. Accepting it elsewhere would read as narrowing the
// targets while changing nothing.
TEST_CASE("ParseTriggeredEffectConfig: factionFilter is rejected on types that ignore it",
          "[effects][parser][triggered]")
{
    CHECK_THROWS_WITH(TriggeredEffectParser::ParseTriggeredEffectConfig(json::parse(R"({
        "type": "GrantEnergy",
        "factionFilter": { "kind": "CouncilMembers" },
        "parameters": { "amount": 500 }
    })"), "on_passed_effects"), ContainsSubstring("factionFilter"));
    CHECK_THROWS_WITH(TriggeredEffectParser::ParseTriggeredEffectConfig(json::parse(R"({
        "type": "Rebel", "factionFilter": { "kind": "CouncilMembers" }
    })"), "on_enter_effects"), ContainsSubstring("SetInfiltration"));
}

// The "belongs in effects" message reads the continuous parser's own table, so a continuous
// type added there is never misreported here as an unknown triggered type.
TEST_CASE("Continuous type detection tracks the continuous parser's table",
          "[effects][parser][triggered]")
{
    for (const char* pType : {"StatModifier", "RuleFlag", "GrantBuilding", "Infiltration",
                              "Conceal", "Detect", "Intercept", "Scramble", "TransportParams",
                              "InteractionOverride", "SocialEngineeringOverride",
                              "DiplomaticModifier", "SocialRatingModifier", "OrbitalAttack"})
    {
        INFO(pType);
        CHECK(EffectConfigParser::IsEffectType(pType));
        CHECK(TriggeredEffectParser::IsContinuousEffectType(pType));
        CHECK_FALSE(TriggeredEffectParser::IsTriggeredEffectType(pType));
    }
    CHECK_FALSE(EffectConfigParser::IsEffectType("Rebel"));
    CHECK_FALSE(EffectConfigParser::IsEffectType("NotAnEffect"));
}

TEST_CASE("ParseTriggeredEffectConfig: GrantXp takes amount and optional condition",
          "[effects][parser][triggered]")
{
    const TriggeredEffectConfig_t config = TriggeredEffectParser::ParseTriggeredEffectConfig(
        json::parse(R"({
            "type": "GrantXp",
            "parameters": { "amount": 2, "op": "Add" },
            "condition": { "kind": "SubjectDomain", "domain": "air" }
        })"), "on_unit_produced_effects");
    const auto* pGrant = std::get_if<GrantXpEffect_t>(&config.effect);
    REQUIRE(pGrant != nullptr);
    CHECK(pGrant->amount == 2);
    CHECK(pGrant->op == ModifierOp_t::Add);
    REQUIRE(config.condition.has_value());
    CHECK(std::holds_alternative<SubjectDomain_t>(*config.condition));

    CHECK_THROWS(TriggeredEffectParser::ParseTriggeredEffectConfig(
        json::parse(R"({ "type": "GrantXp", "parameters": {} })"),
        "on_unit_produced_effects"));
}

TEST_CASE("ParseTriggeredEffectConfig: GrantXp remove_host_chance and RestoreHitPoints",
          "[effects][parser][triggered]")
{
    const TriggeredEffectConfig_t grant = TriggeredEffectParser::ParseTriggeredEffectConfig(
        json::parse(R"({
            "type": "GrantXp",
            "parameters": { "amount": 1, "op": "Add", "remove_host_chance": "1/32" }
        })"), "on_visit_effects");
    const auto* pGrant = std::get_if<GrantXpEffect_t>(&grant.effect);
    REQUIRE(pGrant != nullptr);
    REQUIRE(pGrant->removeHostChance.has_value());
    CHECK(pGrant->removeHostChance->numerator == 1);
    CHECK(pGrant->removeHostChance->denominator == 32);

    const TriggeredEffectConfig_t heal = TriggeredEffectParser::ParseTriggeredEffectConfig(
        json::parse(R"({
            "type": "RestoreHitPoints",
            "parameters": { "amount": 100, "op": "SetPercent" }
        })"), "on_visit_effects");
    const auto* pHeal = std::get_if<RestoreHitPointsEffect_t>(&heal.effect);
    REQUIRE(pHeal != nullptr);
    CHECK(pHeal->amount == 100);
    CHECK(pHeal->op == RestoreHitPointsOp_t::SetPercent);

    CHECK_THROWS(TriggeredEffectParser::ParseTriggeredEffectConfig(
        json::parse(R"({
            "type": "RestoreHitPoints",
            "parameters": { "amount": 1, "op": "MultiplyGeometric" }
        })"), "on_visit_effects"));
}

TEST_CASE("ParseTriggeredEffectConfig: GrantUnit takes component ids", "[effects][parser][triggered]")
{
    const TriggeredEffectConfig_t config = TriggeredEffectParser::ParseTriggeredEffectConfig(
        json::parse(R"({
            "type": "GrantUnit",
            "parameters": { "component_ids": ["Scout", "Infantry"], "count": 2 }
        })"), "on_complete_effects");
    const auto* pGrant = std::get_if<GrantUnitEffect_t>(&config.effect);
    REQUIRE(pGrant != nullptr);
    CHECK(pGrant->componentIds == std::vector<std::string>{"Scout", "Infantry"});
    CHECK(pGrant->count == 2);

    const TriggeredEffectConfig_t defaultCount =
        TriggeredEffectParser::ParseTriggeredEffectConfig(json::parse(R"({
            "type": "GrantUnit", "parameters": { "component_ids": ["Scout"] }
        })"), "on_complete_effects");
    CHECK(std::get<GrantUnitEffect_t>(defaultCount.effect).count == 1);

    CHECK_THROWS(TriggeredEffectParser::ParseTriggeredEffectConfig(
        json::parse(R"({ "type": "GrantUnit", "parameters": { "component_ids": [] } })"),
        "on_complete_effects"));
    CHECK_THROWS(TriggeredEffectParser::ParseTriggeredEffectConfig(json::parse(R"({
        "type": "GrantUnit", "parameters": { "component_ids": ["Scout"], "count": 0 }
    })"), "on_complete_effects"));
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
    CHECK_THROWS(TriggeredEffectParser::ParseTriggeredEffectConfig(
        json::parse(R"({ "type": "AddBuilding", "parameters": {} })"), "on_complete_effects"));
    CHECK_THROWS(TriggeredEffectParser::ParseTriggeredEffectConfig(
        json::parse(R"({ "type": "GrantTech", "parameters": {} })"), "on_complete_effects"));
    CHECK_THROWS(TriggeredEffectParser::ParseTriggeredEffectConfig(
        json::parse(R"({ "type": "GrantUnit", "parameters": {} })"), "on_complete_effects"));

    const TriggeredEffectConfig_t techConfig = TriggeredEffectParser::ParseTriggeredEffectConfig(
        json::parse(R"({ "type": "GrantTech", "parameters": { "tech_id": "biogenetics" } })"),
        "on_complete_effects");
    const auto* pTech = std::get_if<GrantTechEffect_t>(&techConfig.effect);
    REQUIRE(pTech != nullptr);
    REQUIRE(pTech->techId.has_value());
    CHECK(*pTech->techId == "biogenetics");

    const TriggeredEffectConfig_t availableConfig =
        TriggeredEffectParser::ParseTriggeredEffectConfig(
            json::parse(R"({
                "type": "GrantTech",
                "parameters": { "selection": "Available" },
                "once_per": { "scope": "world", "key": "first_bonus" }
            })"),
            "on_discover_effects");
    const auto* pAvailable = std::get_if<GrantTechEffect_t>(&availableConfig.effect);
    REQUIRE(pAvailable != nullptr);
    CHECK_FALSE(pAvailable->techId.has_value());
    REQUIRE(availableConfig.oncePer.has_value());
    CHECK(availableConfig.oncePer->scope == OnceScope_t::World);

    CHECK_THROWS(TriggeredEffectParser::ParseTriggeredEffectConfig(
        json::parse(R"({
            "type": "GrantTech",
            "parameters": { "tech_id": "biogenetics", "selection": "Available" }
        })"),
        "on_discover_effects"));
    CHECK_THROWS(TriggeredEffectParser::ParseTriggeredEffectConfig(
        json::parse(R"({
            "type": "GrantTech",
            "parameters": { "selection": "Everything" }
        })"),
        "on_discover_effects"));
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

TEST_CASE("ParseEffectConfig: InteractionOverride and AttackerIsEmbarked",
          "[effects][parser][interaction]")
{
    const json enterJson = json::parse(R"({
        "type": "InteractionOverride", "scope": "ThisUnit",
        "parameters": {
            "grid": "enter", "actor_domain": "land", "surface": "water", "cell": "allow"
        },
        "condition": { "kind": "AllOf", "values": ["Water", "Base"] }
    })");
    const EffectConfig_t enterConfig = EffectConfigParser::ParseEffectConfig(enterJson);
    const auto* pEnter = std::get_if<InteractionOverrideEffect_t>(&enterConfig.effect);
    REQUIRE(pEnter != nullptr);
    CHECK(pEnter->grid == InteractionGridId_t::Enter);
    CHECK(pEnter->cell == InteractionCell_t::Allow);
    REQUIRE(pEnter->actorDomain.has_value());
    CHECK(*pEnter->actorDomain == UnitDomain_t::Land);
    REQUIRE(pEnter->surface.has_value());
    CHECK(*pEnter->surface == InteractionSurface_t::Water);
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

    const json attackUnitJson = json::parse(R"({
        "type": "InteractionOverride", "scope": "ThisUnit",
        "parameters": { "grid": "attack_unit", "target_domain": "air", "cell": "allow" }
    })");
    const EffectConfig_t attackUnitConfig =
        EffectConfigParser::ParseEffectConfig(attackUnitJson);
    const auto* pAttackUnit =
        std::get_if<InteractionOverrideEffect_t>(&attackUnitConfig.effect);
    REQUIRE(pAttackUnit != nullptr);
    CHECK(pAttackUnit->grid == InteractionGridId_t::AttackUnit);
    REQUIRE(pAttackUnit->targetDomain.has_value());
    CHECK(*pAttackUnit->targetDomain == UnitDomain_t::Air);
    CHECK_FALSE(pAttackUnit->actorDomain.has_value());

    const json zocDenyJson = json::parse(R"({
        "type": "InteractionOverride", "scope": "ThisUnit",
        "parameters": { "grid": "zoc", "cell": "deny" }
    })");
    const EffectConfig_t zocDenyConfig =
        EffectConfigParser::ParseEffectConfig(zocDenyJson);
    const auto* pZocDeny =
        std::get_if<InteractionOverrideEffect_t>(&zocDenyConfig.effect);
    REQUIRE(pZocDeny != nullptr);
    CHECK(pZocDeny->grid == InteractionGridId_t::Zoc);
    CHECK(pZocDeny->cell == InteractionCell_t::Deny);

    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "InteractionOverride", "scope": "ThisUnit",
        "parameters": { "grid": "enter", "target_domain": "air", "cell": "allow" }
    })")));
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "InteractionOverride", "scope": "ThisUnit",
        "parameters": { "grid": "attack_unit", "surface": "water", "cell": "allow" }
    })")));
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "Permission", "scope": "ThisUnit",
        "parameters": { "permission": "EnterTile" }
    })")));
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "InteractionOverride", "scope": "ThisUnit",
        "parameters": { "grid": "enter" }
    })")));
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "InteractionOverride", "scope": "ThisUnit",
        "parameters": { "grid": "zoc", "cell": "maybe" }
    })")));

    // Tile-scoped overrides are rejected: resolve only consults the acting unit.
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "InteractionOverride", "scope": "ThisTile",
        "parameters": { "grid": "attack_unit", "target_domain": "air", "cell": "allow" }
    })")));
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "InteractionOverride", "scope": "ThisBase",
        "parameters": { "grid": "enter", "surface": "water", "cell": "allow" }
    })")));

    // attack_tile shares actor_domain with every grid but owns the footing column.
    const json attackTileJson = json::parse(R"({
        "type": "InteractionOverride", "scope": "ThisUnit",
        "parameters": { "grid": "attack_tile", "footing": "embarked", "cell": "allow" }
    })");
    const EffectConfig_t attackTileConfig =
        EffectConfigParser::ParseEffectConfig(attackTileJson);
    const auto* pAttackTile =
        std::get_if<InteractionOverrideEffect_t>(&attackTileConfig.effect);
    REQUIRE(pAttackTile != nullptr);
    CHECK(pAttackTile->grid == InteractionGridId_t::AttackTile);
    CHECK(pAttackTile->footing == InteractionFooting_t::Embarked);
    CHECK(pAttackTile->cell == InteractionCell_t::Allow);
    CHECK_FALSE(pAttackTile->actorDomain.has_value());

    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "InteractionOverride", "scope": "ThisUnit",
        "parameters": { "grid": "attack_tile", "target_domain": "air", "cell": "allow" }
    })")));
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "InteractionOverride", "scope": "ThisUnit",
        "parameters": { "grid": "enter", "footing": "embarked", "cell": "allow" }
    })")));
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "InteractionOverride", "scope": "ThisUnit",
        "parameters": { "grid": "attack_tile", "footing": "sideways", "cell": "allow" }
    })")));

    const json embarkedJson = json::parse(R"({
        "type": "InteractionOverride", "scope": "ThisUnit",
        "parameters": { "grid": "attack_unit", "cell": "allow" },
        "condition": { "kind": "AttackerIsEmbarked" }
    })");
    const EffectConfig_t embarkedConfig = EffectConfigParser::ParseEffectConfig(embarkedJson);
    REQUIRE(embarkedConfig.condition.has_value());
    CHECK(std::holds_alternative<AttackerIsEmbarked_t>(embarkedConfig.condition->AsVariant()));

    const json factionUnitsJson = json::parse(R"({
        "type": "InteractionOverride", "scope": "FactionUnits",
        "parameters": { "grid": "enter", "actor_domain": "land", "surface": "water",
                       "cell": "allow" }
    })");
    CHECK(std::holds_alternative<InteractionOverrideEffect_t>(
        EffectConfigParser::ParseEffectConfig(factionUnitsJson).effect));

    const json actorDomainJson = json::parse(R"({
        "type": "StatModifier", "scope": "ThisUnit",
        "parameters": { "stat": "defense", "amount": 100, "op": "AddPercent" },
        "condition": { "kind": "AttackerDomain", "domains": ["air", "orbital"] }
    })");
    const EffectConfig_t domainConfig =
        EffectConfigParser::ParseEffectConfig(actorDomainJson);
    REQUIRE(domainConfig.condition.has_value());
    const auto* pDomains =
        std::get_if<AttackerDomain_t>(&domainConfig.condition->AsVariant());
    REQUIRE(pDomains);
    REQUIRE(pDomains->domains.size() == 2);
    CHECK(pDomains->domains[0] == UnitDomain_t::Air);
    CHECK(pDomains->domains[1] == UnitDomain_t::Orbital);

    CHECK_THROWS(EffectConfigParser::ParseCondition(
        json::parse(R"({ "kind": "AttackerDomain" })")));
    CHECK_THROWS(EffectConfigParser::ParseCondition(
        json::parse(R"({ "kind": "AttackerDomain", "domains": [] })")));

    const json defenderDomainJson = json::parse(R"({
        "type": "StatModifier", "scope": "ThisUnit",
        "parameters": { "stat": "attack", "amount": 100, "op": "AddPercent" },
        "condition": { "kind": "DefenderDomain", "domains": ["air", "orbital"] }
    })");
    const EffectConfig_t defenderDomainConfig =
        EffectConfigParser::ParseEffectConfig(defenderDomainJson);
    REQUIRE(defenderDomainConfig.condition.has_value());
    const auto* pDefenderDomains =
        std::get_if<DefenderDomain_t>(&defenderDomainConfig.condition->AsVariant());
    REQUIRE(pDefenderDomains);
    REQUIRE(pDefenderDomains->domains.size() == 2);
    CHECK(pDefenderDomains->domains[0] == UnitDomain_t::Air);
    CHECK(pDefenderDomains->domains[1] == UnitDomain_t::Orbital);

    CHECK_THROWS(EffectConfigParser::ParseCondition(
        json::parse(R"({ "kind": "DefenderDomain" })")));
    CHECK_THROWS(EffectConfigParser::ParseCondition(
        json::parse(R"({ "kind": "DefenderDomain", "domains": [] })")));

    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(
        json::parse(R"({ "type": "Permission", "scope": "ThisUnit", "parameters": {} })")));
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "Permission", "scope": "ThisUnit",
        "parameters": { "permission": "Fly" }
    })")));
}

TEST_CASE("ParseEffectConfig: tile MaxClamp and bypass_clamp", "[effects][parser]")
{
    const json clampJson = json::parse(R"({
        "type": "StatModifier", "scope": "FactionGlobal",
        "removed_by_tech": "gene_splicing",
        "parameters": {
            "stat": "nutrients", "amount": 2, "op": "MaxClamp",
            "selector": { "kind": "AnyTile" }
        }
    })");
    const EffectConfig_t clampConfig = EffectConfigParser::ParseEffectConfig(clampJson);
    const auto* pClamp = std::get_if<StatModifierEffect_t>(&clampConfig.effect);
    REQUIRE(pClamp != nullptr);
    CHECK(pClamp->stat == StatId_t::Nutrients);
    CHECK(pClamp->amount == Approx(2.0));
    CHECK(pClamp->op == ModifierOp_t::MaxClamp);
    REQUIRE(pClamp->selector.has_value());
    CHECK(std::holds_alternative<TileSelectorAnyTile_t>(*pClamp->selector));
    CHECK(clampConfig.removedByTech == "gene_splicing");

    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "StatModifier", "scope": "FactionGlobal",
        "parameters": { "stat": "nutrients", "amount": 2, "op": "MaxClamp" }
    })")));

    const json bypassJson = json::parse(R"({
        "type": "StatModifier", "scope": "ThisTile",
        "parameters": { "stat": "nutrients", "amount": 2, "op": "Add", "bypass_clamp": true }
    })");
    const EffectConfig_t bypassConfig = EffectConfigParser::ParseEffectConfig(bypassJson);
    const auto* pMod = std::get_if<StatModifierEffect_t>(&bypassConfig.effect);
    REQUIRE(pMod != nullptr);
    CHECK(pMod->bypassClamp);

    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "StatModifier", "scope": "ThisTile",
        "parameters": {
            "stat": "nutrients", "amount": 2, "op": "AddPercent",
            "bypass_clamp": true
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
        "factionFilter": { "kind": "CouncilMembers" }
    })"));
    CHECK(council.scope == EffectScope_t::FactionGlobal);
    CHECK(std::get_if<InfiltrationEffect_t>(&council.effect));
    REQUIRE(council.factionFilter);
    CHECK(council.factionFilter->kind == FactionFilterKind_t::CouncilMembers);

    const EffectConfig_t world = EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "Infiltration",
        "scope": "WorldGlobal"
    })"));
    CHECK(world.scope == EffectScope_t::WorldGlobal);
    CHECK_FALSE(world.factionFilter.has_value());

    const EffectConfig_t aiFilter = EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "Infiltration",
        "scope": "FactionGlobal",
        "factionFilter": { "kind": "PlayerType", "type": "AI" }
    })"));
    REQUIRE(aiFilter.factionFilter);
    CHECK(aiFilter.factionFilter->kind == FactionFilterKind_t::PlayerType);
    CHECK(aiFilter.factionFilter->playerType == PlayerType_t::AI);

    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "Infiltration", "scope": "FactionGlobal"
    })")));
    // Only a probe mission supplies an action target, and a mission fires a triggered list.
    CHECK_THROWS_WITH(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "Infiltration",
        "scope": "FactionGlobal",
        "factionFilter": { "kind": "ActionTarget" }
    })")), ContainsSubstring("SetInfiltration"));
}

TEST_CASE("ParseTriggeredEffectConfig: SetInfiltration takes a bare factionFilter",
          "[effects][parser][triggered]")
{
    const TriggeredEffectConfig_t probe = TriggeredEffectParser::ParseTriggeredEffectConfig(
        json::parse(R"({
            "type": "SetInfiltration",
            "factionFilter": { "kind": "ActionTarget" }
        })"), "on_success_effects");
    REQUIRE(std::get_if<SetInfiltrationEffect_t>(&probe.effect) != nullptr);
    REQUIRE(probe.factionFilter);
    CHECK(probe.factionFilter->kind == FactionFilterKind_t::ActionTarget);

    // No scope to consult, so an absent filter means every other faction rather than an error.
    const TriggeredEffectConfig_t all = TriggeredEffectParser::ParseTriggeredEffectConfig(
        json::parse(R"({ "type": "SetInfiltration" })"), "on_elected_effects");
    CHECK_FALSE(all.factionFilter.has_value());
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

TEST_CASE("ParseEffectConfig: per-effect min_radius", "[effects][parser][radius]")
{
    SECTION("parsed from the effect entry, default 0")
    {
        const json ring = json::parse(R"({
            "type": "StatModifier", "scope": "ThisTile", "radius": 1, "min_radius": 1,
            "parameters": { "stat": "energy", "amount": 1 }
        })");
        CHECK(EffectConfigParser::ParseEffectConfig(ring).minRadius == 1);

        const json plain = json::parse(R"({
            "type": "StatModifier", "scope": "ThisTile", "radius": 1,
            "parameters": { "stat": "energy", "amount": 1 }
        })");
        CHECK(EffectConfigParser::ParseEffectConfig(plain).minRadius == 0);
    }

    SECTION("negative min_radius throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier", "scope": "ThisTile", "radius": 1, "min_radius": -1,
            "parameters": { "stat": "energy", "amount": 1 }
        })")));
    }

    SECTION("min_radius above radius throws — the effect would reach nothing")
    {
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier", "scope": "ThisTile", "radius": 1, "min_radius": 2,
            "parameters": { "stat": "energy", "amount": 1 }
        })")));
        // Bare min_radius with no radius is the same mistake: radius defaults to 0.
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier", "scope": "ThisTile", "min_radius": 1,
            "parameters": { "stat": "energy", "amount": 1 }
        })")));
    }

    SECTION("nonzero min_radius on non-ThisTile throws")
    {
        CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
            "type": "StatModifier", "scope": "ThisBase", "min_radius": 1,
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
        "type": "Intercept", "scope": "FactionGlobal",
        "parameters": {},
        "condition": { "kind": "AttackerDomain", "domains": ["orbital"] }
    })")));

    const EffectConfig_t interceptNoCooldown = EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "Intercept", "scope": "FactionGlobal",
        "parameters": { "chance": 50 },
        "condition": { "kind": "AttackerDomain", "domains": ["orbital"] }
    })"));
    const auto* pIntercept = std::get_if<InterceptEffect_t>(&interceptNoCooldown.effect);
    REQUIRE(pIntercept);
    CHECK(pIntercept->chance == 50);
    CHECK(pIntercept->cooldownTurns == -1);

    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "Scramble", "scope": "ThisUnit"
    })")));
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "Scramble", "scope": "ThisUnit",
        "condition": { "kind": "AttackerDomain", "domains": ["air"] }
    })")));
    CHECK_THROWS(EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "Scramble", "scope": "ThisUnit",
        "parameters": { "range": 0 },
        "condition": { "kind": "AttackerDomain", "domains": ["air"] }
    })")));
    const EffectConfig_t scramble = EffectConfigParser::ParseEffectConfig(json::parse(R"({
        "type": "Scramble", "scope": "ThisUnit",
        "parameters": { "range": 2 },
        "condition": { "kind": "AttackerDomain", "domains": ["air"] }
    })"));
    const auto* pScramble = std::get_if<ScrambleEffect_t>(&scramble.effect);
    REQUIRE(pScramble);
    CHECK(pScramble->range == 2);
}

TEST_CASE("ParseEffects: non-array effects throws", "[effects][parser]")
{
    CHECK_THROWS(EffectConfigParser::ParseEffects(json::parse(R"({
        "id": "bad",
        "effects": { "type": "StatModifier", "scope": "ThisBase" }
    })")));
}

namespace
{

// ValidateEffectForSource takes the whole effect (it also checks amount_source). These cases
// only vary scope, so they build a default StatModifier carrying it.
void ValidateScopeForSource_(EffectScope_t scope, EffectSourceKind_t sourceKind,
                             const std::string& rSourceId)
{
    EffectConfig_t effect;
    effect.scope = scope;
    EffectConfigParser::ValidateEffectForSource(effect, sourceKind, rSourceId);
}

} // namespace

TEST_CASE("ValidateEffectForSource: rejects only the certainly-impossible combinations",
          "[effects][parser][validation]")
{
    // ThisPop can only ever resolve against a pop type; ThisUnit against a unit component
    // or morale level (combat rank effects folded in at resolve time).
    CHECK_THROWS(ValidateScopeForSource_(
        EffectScope_t::ThisPop, EffectSourceKind_t::Building, "some_building"));
    CHECK_THROWS(ValidateScopeForSource_(
        EffectScope_t::ThisUnit, EffectSourceKind_t::PopType, "some_pop"));

    CHECK_NOTHROW(ValidateScopeForSource_(
        EffectScope_t::ThisPop, EffectSourceKind_t::PopType, "some_pop"));
    CHECK_NOTHROW(ValidateScopeForSource_(
        EffectScope_t::ThisUnit, EffectSourceKind_t::UnitComponent, "some_component"));
    CHECK_NOTHROW(ValidateScopeForSource_(
        EffectScope_t::ThisUnit, EffectSourceKind_t::MoraleLevel, "morale_level_4"));
    CHECK_THROWS(ValidateScopeForSource_(
        EffectScope_t::ThisBase, EffectSourceKind_t::MoraleLevel, "morale_level_4"));
    CHECK_NOTHROW(ValidateScopeForSource_(
        EffectScope_t::ThisUnit, EffectSourceKind_t::NativeUnit, "Mind_Worm"));
    CHECK_THROWS(ValidateScopeForSource_(
        EffectScope_t::ThisBase, EffectSourceKind_t::NativeUnit, "Mind_Worm"));

    // ThisBase / ProducedAtThisBase need an origin base (or pop-merge path). A unit component
    // or probe action that wants to act on the production / mission base at the moment the
    // trigger fires uses a triggered list, which carries the base in its context instead.
    CHECK_THROWS(ValidateScopeForSource_(
        EffectScope_t::ThisBase, EffectSourceKind_t::UnitComponent, "sensor_pod"));
    CHECK_THROWS(ValidateScopeForSource_(
        EffectScope_t::ThisBase, EffectSourceKind_t::ProbeAction, "genetic_plague"));
    CHECK_THROWS(ValidateScopeForSource_(
        EffectScope_t::ProducedAtThisBase, EffectSourceKind_t::UnitComponent, "colony_pod"));
    CHECK_THROWS(ValidateScopeForSource_(
        EffectScope_t::ProducedAtThisBase, EffectSourceKind_t::Improvement, "monolith"));
    CHECK_THROWS(ValidateScopeForSource_(
        EffectScope_t::ThisBase, EffectSourceKind_t::CouncilProposal, "trade_pact"));
    CHECK_THROWS(ValidateScopeForSource_(
        EffectScope_t::ThisBase, EffectSourceKind_t::TileYieldRules, "tile_yield_rules"));
    CHECK_THROWS(ValidateScopeForSource_(
        EffectScope_t::ThisBase, EffectSourceKind_t::Production, "production"));
    CHECK_NOTHROW(ValidateScopeForSource_(
        EffectScope_t::ThisBase, EffectSourceKind_t::Building, "recycling_tanks"));
    CHECK_NOTHROW(ValidateScopeForSource_(
        EffectScope_t::ProducedAtThisBase, EffectSourceKind_t::Building, "aerospace"));

    // pop_composition splits into two source kinds because they have opposite origin-base
    // capabilities. The faction-wide `effects` array enters the pool with no origin base, so
    // ThisBase there would never resolve; the per-base mood arrays (riot_tiers,
    // golden_age_effects) are collected against a specific base, so ThisBase is exactly how a
    // riot tier declares its resource clamps.
    CHECK_THROWS(ValidateScopeForSource_(
        EffectScope_t::ThisBase, EffectSourceKind_t::PopComposition, "pop_composition"));
    CHECK_NOTHROW(ValidateScopeForSource_(
        EffectScope_t::ThisBase, EffectSourceKind_t::PopCompositionBaseLocal,
        "pop_composition.riot_tiers.effects"));
    CHECK_NOTHROW(ValidateScopeForSource_(
        EffectScope_t::FactionUnits, EffectSourceKind_t::PopCompositionBaseLocal,
        "pop_composition.riot_tiers.effects"));
    // Neither collector walks ProducedAtThisBase off a mood array.
    CHECK_THROWS(ValidateScopeForSource_(
        EffectScope_t::ProducedAtThisBase, EffectSourceKind_t::PopCompositionBaseLocal,
        "pop_composition.riot_tiers.effects"));

    // Legal-but-inert: faction-lane on improvement (pending territory) still loads.
    CHECK_NOTHROW(ValidateScopeForSource_(
        EffectScope_t::FactionGlobal, EffectSourceKind_t::Improvement, "monolith"));
    CHECK_NOTHROW(ValidateScopeForSource_(
        EffectScope_t::ThisTile, EffectSourceKind_t::UnitComponent, "sensor_pod"));
    CHECK_NOTHROW(ValidateScopeForSource_(
        EffectScope_t::WorldGlobal, EffectSourceKind_t::Building, "beacon"));

    // ThisTech is tech-config-local research cost modifiers only.
    CHECK_THROWS(ValidateScopeForSource_(
        EffectScope_t::ThisTech, EffectSourceKind_t::Building, "some_building"));
    CHECK_THROWS(ValidateScopeForSource_(
        EffectScope_t::ThisTech, EffectSourceKind_t::Tech, "cheap_tech"));
}

TEST_CASE("ValidateEffectForSource: ThisTech accepts only tech_cost StatModifiers",
          "[effects][parser][validation]")
{
    EffectConfig_t techCost;
    techCost.scope = EffectScope_t::ThisTech;
    techCost.effect = StatModifierEffect_t{StatId_t::TechCost, 50.0, ModifierOp_t::Add};
    CHECK_NOTHROW(EffectConfigParser::ValidateEffectForSource(
        techCost, EffectSourceKind_t::Tech, "pricey_tech"));

    EffectConfig_t wrongStat = techCost;
    wrongStat.effect = StatModifierEffect_t{StatId_t::Labs, 1.0, ModifierOp_t::Add};
    CHECK_THROWS_WITH(
        EffectConfigParser::ValidateEffectForSource(
            wrongStat, EffectSourceKind_t::Tech, "labs_tech"),
        Catch::Matchers::ContainsSubstring("ThisTech")
            && Catch::Matchers::ContainsSubstring("tech_cost"));

    CHECK_THROWS_WITH(
        EffectConfigParser::ValidateEffectForSource(
            techCost, EffectSourceKind_t::Faction, "gaians"),
        Catch::Matchers::ContainsSubstring("ThisTech")
            && Catch::Matchers::ContainsSubstring("tech"));
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
