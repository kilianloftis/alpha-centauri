#include "game/diplomacy/DiplomacyConfigParser.h"
#include "lib/config/ConfigSets.h"
#include "lib/config/JsonConfigLoader.h"
#include "game/effects/EffectConfigParser.h"
#include "game/effects/EffectEnums.h"
#include "game/diplomacy/DiplomaticTransitionRules.h"
#include "game/map/MapUtils.h"
#include "lib/Rational.h"
#include <magic_enum.hpp>
#include <string>
#include <variant>
#include <vector>

namespace ac
{

namespace
{

// An exact non-negative factor: an integer or a fraction string ("1/3").
Rational_t RequireFactor_(const ConfigReader& rReader, const char* pKey)
{
    const Rational_t factor = rReader.Rational(pKey);
    if (factor.numerator < 0)
    {
        rReader.FailKey(pKey, "must not be negative");
    }
    return factor;
}

// The stats some rule resolves toward a partner; a status effect on any other would be ignored.
bool IsPartnerStat_(StatId_t stat)
{
    return stat == StatId_t::CommerceRate || stat == StatId_t::ProbeActorCost;
}

// A probe surcharge may be limited to base quotes (Pact's); nothing else reads a condition here.
bool ConditionAllowed_(const EffectConfig_t& rEffect, StatId_t stat)
{
    if (!rEffect.condition)
    {
        return true;
    }
    return stat == StatId_t::ProbeActorCost
        && std::holds_alternative<TargetIsBase_t>(*rEffect.condition);
}

std::vector<EffectConfig_t> ParseStatusEffects_(const ConfigReader& rReader,
                                                DiplomaticStatus_t status)
{
    const std::string sourceId = "statuses." + std::string(magic_enum::enum_name(status));
    std::vector<EffectConfig_t> effects = EffectConfigParser::ParseEffects(
        rReader.Json(), EffectSourceKind_t::DiplomaticStatus, sourceId);
    for (const EffectConfig_t& rEffect : effects)
    {
        const auto* pModifier = std::get_if<StatModifierEffect_t>(&rEffect.effect);
        if (!pModifier || !IsPartnerStat_(pModifier->stat) || pModifier->amountSource
            || !ConditionAllowed_(rEffect, pModifier->stat))
        {
            rReader.FailKey("effects",
                            "may only be StatModifier commerce_rate entries, or "
                            "probe_actor_cost ones optionally limited to TargetIsBase");
        }
    }
    return effects;
}

std::optional<int> RequireOptionalPositiveInt_(const ConfigReader& rReader, const char* pKey)
{
    if (rReader.Require(pKey).is_null())
    {
        return std::nullopt;
    }
    return rReader.IntAtLeast(pKey, 1);
}

DiplomaticStatusRules_t ParseStatus_(const ConfigReader& rReader, DiplomaticStatus_t status)
{
    rReader.RejectUnknownKeys({
        "enter_territory",      "share_tiles", "repair_at_bases", "may_attack",
        "break_by_attack",      "defensive_obligation", "effects", "duration_turns",
    });

    DiplomaticStatusRules_t rules;
    rules.bEnterTerritory = rReader.Bool("enter_territory");
    rules.bShareTiles = rReader.Bool("share_tiles");
    rules.bRepairAtBases = rReader.Bool("repair_at_bases");
    rules.bMayAttack = rReader.Bool("may_attack");
    rules.bBreakByAttack = rReader.Bool("break_by_attack");
    rules.bDefensiveObligation = rReader.Bool("defensive_obligation");
    rules.effects = ParseStatusEffects_(rReader, status);
    rules.durationTurns = RequireOptionalPositiveInt_(rReader, "duration_turns");
    if (rules.durationTurns && !StepDown(status))
    {
        rReader.FailKey("duration_turns", "needs a status to expire into");
    }
    if (rules.bDefensiveObligation && !StepDown(status))
    {
        rReader.FailKey("defensive_obligation",
                        "needs a status to step down into when the obligation is declined");
    }
    return rules;
}

BaseValueConfig_t ParseBaseValue_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys(
        {"scan_radius", "credits_offset", "distance_offset", "min_population", "minimum",
         "ungarrisoned_factor", "counterparty_absent_multiplier", "lone_base_multiplier",
         "outnumbered_ratio", "outnumbered_factor", "minerals_per_row",
         "project_rows_multiplier", "unit_rows_multiplier"});
    BaseValueConfig_t config;
    config.scanRadius = rReader.IntInRange("scan_radius", 1, k_TableOffsetsMaxRadius);
    config.creditsOffset = rReader.IntAtLeast("credits_offset", 0);
    config.distanceOffset = rReader.IntAtLeast("distance_offset", 1);
    config.minPopulation = rReader.IntAtLeast("min_population", 0);
    config.minimum = rReader.IntAtLeast("minimum", 0);
    config.ungarrisonedFactor = RequireFactor_(rReader, "ungarrisoned_factor");
    config.counterpartyAbsentMultiplier =
        rReader.IntAtLeast("counterparty_absent_multiplier", 1);
    config.loneBaseMultiplier = rReader.IntAtLeast("lone_base_multiplier", 1);
    config.outnumberedRatio = rReader.IntAtLeast("outnumbered_ratio", 1);
    config.outnumberedFactor = RequireFactor_(rReader, "outnumbered_factor");
    config.mineralsPerRow = rReader.IntAtLeast("minerals_per_row", 1);
    config.projectRowsMultiplier = rReader.IntAtLeast("project_rows_multiplier", 0);
    config.unitRowsMultiplier = rReader.IntAtLeast("unit_rows_multiplier", 0);
    return config;
}

GiftSizeStepConfig_t ParseGiftSizeStep_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"size", "recipient_credits_factor", "giver_credits_factor",
                               "turn_factor", "minimum"});
    GiftSizeStepConfig_t step;
    step.size = rReader.Enum<GiftSize_t>("size", "gift size");
    step.recipientCreditsFactor = RequireFactor_(rReader, "recipient_credits_factor");
    step.giverCreditsFactor = RequireFactor_(rReader, "giver_credits_factor");
    step.turnFactor = RequireFactor_(rReader, "turn_factor");
    step.minimum = rReader.IntAtLeast("minimum", 0);
    return step;
}

// The last rung has no thresholds: it is the size when no other rung is reached.
GiftSizesConfig_t ParseGiftSizes_(const ConfigReader& rReader)
{
    rReader.NonEmptyArray("sizes");
    const std::vector<ConfigReader> rungs = rReader.Objects("sizes");
    GiftSizesConfig_t sizes;
    for (std::size_t i = 0; i + 1 < rungs.size(); ++i)
    {
        sizes.steps.push_back(ParseGiftSizeStep_(rungs[i]));
    }
    rungs.back().RejectUnknownKeys({"size"});
    sizes.fallback = rungs.back().Enum<GiftSize_t>("size", "gift size");
    return sizes;
}

CreditFrictionConfig_t ParseCreditFriction_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"credits_per_unit", "first_block_units", "first_block_divisor",
                               "block_units", "units_per_point", "factor"});
    CreditFrictionConfig_t config;
    config.creditsPerUnit = rReader.IntAtLeast("credits_per_unit", 1);
    config.firstBlockUnits = rReader.IntAtLeast("first_block_units", 0);
    config.firstBlockDivisor = rReader.IntAtLeast("first_block_divisor", 1);
    config.blockUnits = rReader.IntAtLeast("block_units", 1);
    config.unitsPerPoint = rReader.IntAtLeast("units_per_point", 1);
    config.factor = RequireFactor_(rReader, "factor");
    return config;
}

// A kind whose friction is its value scaled by one factor.
Rational_t ParseFrictionFactor_(const ConfigReader& rReader)
{
    const ConfigReader friction = rReader.Object("friction");
    friction.RejectUnknownKeys({"factor"});
    return RequireFactor_(friction, "factor");
}

CreditGiftConfig_t ParseCreditGift_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"sizes", "friction"});
    CreditGiftConfig_t config;
    config.sizes = ParseGiftSizes_(rReader);
    config.friction = ParseCreditFriction_(rReader.Object("friction"));
    return config;
}

TechValueConfig_t ParseTechValue_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"priority_factor", "unmatched_factor", "growth_base_count",
                               "extra_owner_penalty", "minimum"});
    TechValueConfig_t config;
    config.priorityFactor = rReader.IntAtLeast("priority_factor", 0);
    config.unmatchedFactor = RequireFactor_(rReader, "unmatched_factor");
    config.growthBaseCount = rReader.IntAtLeast("growth_base_count", 0);
    config.extraOwnerPenalty = rReader.IntAtLeast("extra_owner_penalty", 0);
    config.minimum = rReader.IntAtLeast("minimum", 0);
    return config;
}

TechGiftConfig_t ParseTechGift_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"sizes", "friction", "value"});
    TechGiftConfig_t config;
    config.sizes = ParseGiftSizes_(rReader);
    config.frictionFactor = ParseFrictionFactor_(rReader);
    config.value = ParseTechValue_(rReader.Object("value"));
    return config;
}

BaseGiftConfig_t ParseBaseGift_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"sizes", "friction"});
    BaseGiftConfig_t config;
    config.sizes = ParseGiftSizes_(rReader);
    config.frictionFactor = ParseFrictionFactor_(rReader);
    return config;
}

GiftConfig_t ParseGifts_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"credits", "technology", "base"});
    GiftConfig_t config;
    config.credits = ParseCreditGift_(rReader.Object("credits"));
    config.technology = ParseTechGift_(rReader.Object("technology"));
    config.base = ParseBaseGift_(rReader.Object("base"));
    return config;
}

DiplomacyConfig_t ParseDiplomacy_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"statuses", "defensive_obligation_mode", "friction_max",
                               "blemishes_max", "break_agreement_blemishes",
                               "probe_keep_friction_draw", "artifact_capture_friction",
                               "allied_territory_battle_friction",
                               "allied_territory_battle_gifts_floor",
                               "terraform_rainfall_friction", "terraform_rainfall_stop_distance",
                               "base_value", "gifts"});

    DiplomacyConfig_t config;
    config.defensiveObligationMode = rReader.Enum<DefensiveObligationMode_t>(
        "defensive_obligation_mode", "defensive obligation mode");
    config.frictionMax = rReader.IntAtLeast("friction_max", 1);
    config.blemishesMax = rReader.IntAtLeast("blemishes_max", 1);
    config.breakAgreementBlemishes = rReader.IntAtLeast("break_agreement_blemishes", 0);
    config.probeKeepFrictionDraw = rReader.IntAtLeast("probe_keep_friction_draw", 1);
    config.artifactCaptureFriction = rReader.IntAtLeast("artifact_capture_friction", 0);
    config.alliedTerritoryBattleFriction = rReader.Int("allied_territory_battle_friction");
    config.alliedTerritoryBattleGiftsFloor =
        rReader.IntAtLeast("allied_territory_battle_gifts_floor", 0);
    config.terraformRainfallFriction = rReader.IntAtLeast("terraform_rainfall_friction", 0);
    config.terraformRainfallStopDistance =
        rReader.IntAtLeast("terraform_rainfall_stop_distance", 0);
    config.baseValue = ParseBaseValue_(rReader.Object("base_value"));
    config.gifts = ParseGifts_(rReader.Object("gifts"));
    config.statuses =
        ConfigFields::ParseEnumKeyedObject<DiplomaticStatus_t, DiplomaticStatusRules_t>(
            rReader.Object("statuses"), "diplomatic status", ParseStatus_);
    return config;
}

} // namespace

DiplomacyConfig_t DiplomacyConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<DiplomacyConfig_t>(configPath, "diplomacy",
                                                               ParseDiplomacy_);
}

} // namespace ac
