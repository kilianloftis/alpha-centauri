#include "game/diplomacy/DiplomacyConfigParser.h"

#include "lib/config/ConfigFields.h"
#include "lib/config/ConfigSets.h"
#include "game/effects/EffectConfigParser.h"
#include "game/effects/EffectEnums.h"
#include "game/diplomacy/DiplomaticTransitionRules.h"
#include "game/map/MapUtils.h"
#include "lib/config/EnumNames.h"
#include "lib/Rational.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace ac
{

namespace
{

const nlohmann::json& Require_(const nlohmann::json& rJson, const std::string& rKey,
                               const std::string& rContext)
{
    if (!rJson.contains(rKey))
    {
        throw std::runtime_error(rContext + " requires '" + rKey + "'");
    }
    return rJson.at(rKey);
}

bool RequireBool_(const nlohmann::json& rJson, const std::string& rKey,
                  const std::string& rContext)
{
    const nlohmann::json& rValue = Require_(rJson, rKey, rContext);
    if (!rValue.is_boolean())
    {
        throw std::runtime_error(rContext + ": '" + rKey + "' must be a boolean");
    }
    return rValue.get<bool>();
}

const nlohmann::json& RequireObject_(const nlohmann::json& rJson, const std::string& rKey,
                                     const std::string& rContext)
{
    const nlohmann::json& rValue = Require_(rJson, rKey, rContext);
    if (!rValue.is_object())
    {
        throw std::runtime_error(rContext + ": '" + rKey + "' must be a JSON object");
    }
    return rValue;
}

// An exact non-negative factor: an integer or a fraction string ("1/3").
Rational_t RequireFactor_(const nlohmann::json& rJson, const std::string& rKey,
                          const std::string& rContext)
{
    const nlohmann::json& rValue = Require_(rJson, rKey, rContext);
    const Rational_t factor = Rational_t::ParseJson(rValue);
    if (factor.numerator < 0)
    {
        throw std::runtime_error(rContext + ": '" + rKey + "' must not be negative");
    }
    return factor;
}

GiftSize_t RequireGiftSize_(const nlohmann::json& rJson, const std::string& rContext)
{
    const nlohmann::json& rValue = Require_(rJson, "size", rContext);
    if (!rValue.is_string())
    {
        throw std::runtime_error(rContext + ": 'size' must be a string");
    }
    return EnumFromName<GiftSize_t>(rValue.get<std::string>(), "gift size");
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

std::vector<EffectConfig_t> ParseStatusEffects_(const nlohmann::json& rJson,
                                                  const std::string& rContext)
{
    std::vector<EffectConfig_t> effects =
        EffectConfigParser::ParseEffects(rJson, EffectSourceKind_t::DiplomaticStatus, rContext);
    for (const EffectConfig_t& rEffect : effects)
    {
        const auto* pModifier = std::get_if<StatModifierEffect_t>(&rEffect.effect);
        if (!pModifier || !IsPartnerStat_(pModifier->stat) || pModifier->amountSource
            || !ConditionAllowed_(rEffect, pModifier->stat))
        {
            throw std::runtime_error(rContext
                                     + ": effects may only be StatModifier commerce_rate "
                                       "entries, or probe_actor_cost ones optionally limited "
                                       "to TargetIsBase");
        }
    }
    return effects;
}

std::optional<int> RequireOptionalPositiveInt_(const nlohmann::json& rJson,
                                               const std::string& rKey,
                                               const std::string& rContext)
{
    const nlohmann::json& rValue = Require_(rJson, rKey, rContext);
    if (rValue.is_null())
    {
        return std::nullopt;
    }
    if (!rValue.is_number_integer() || rValue.get<int>() <= 0)
    {
        throw std::runtime_error(rContext + ": '" + rKey + "' must be null or an integer > 0");
    }
    return rValue.get<int>();
}

DiplomaticStatusRules_t ParseStatus_(const nlohmann::json& rJson, DiplomaticStatus_t status,
                                     const std::string& rContext)
{
    if (!rJson.is_object())
    {
        throw std::runtime_error(rContext + " must be a JSON object");
    }
    static const std::vector<std::string> known = {
        "enter_territory",      "share_tiles", "repair_at_bases", "may_attack",
        "break_by_attack",      "defensive_obligation", "effects", "duration_turns",
    };
    ConfigFields::RejectUnknownKeys(rJson, known, rContext);

    DiplomaticStatusRules_t rules;
    rules.bEnterTerritory = RequireBool_(rJson, "enter_territory", rContext);
    rules.bShareTiles = RequireBool_(rJson, "share_tiles", rContext);
    rules.bRepairAtBases = RequireBool_(rJson, "repair_at_bases", rContext);
    rules.bMayAttack = RequireBool_(rJson, "may_attack", rContext);
    rules.bBreakByAttack = RequireBool_(rJson, "break_by_attack", rContext);
    rules.bDefensiveObligation = RequireBool_(rJson, "defensive_obligation", rContext);
    rules.effects = ParseStatusEffects_(rJson, rContext);
    rules.durationTurns = RequireOptionalPositiveInt_(rJson, "duration_turns", rContext);
    if (rules.durationTurns && !StepDown(status))
    {
        throw std::runtime_error(rContext + ": 'duration_turns' needs a status to expire into");
    }
    if (rules.bDefensiveObligation && !StepDown(status))
    {
        throw std::runtime_error(rContext
                                 + ": 'defensive_obligation' needs a status to step down into "
                                   "when the obligation is declined");
    }
    return rules;
}

BaseValueConfig_t ParseBaseValue_(const nlohmann::json& rJson, const std::string& rContext)
{
    ConfigFields::RejectUnknownKeys(
        rJson,
        {"scan_radius", "credits_offset", "distance_offset", "min_population", "minimum",
         "ungarrisoned_factor", "counterparty_absent_multiplier", "lone_base_multiplier",
         "outnumbered_ratio", "outnumbered_factor", "minerals_per_row",
         "project_rows_multiplier", "unit_rows_multiplier"},
        rContext);
    BaseValueConfig_t config;
    config.scanRadius = ConfigFields::RequireIntAtLeast(rJson, "scan_radius", 1, rContext);
    if (config.scanRadius > k_TableOffsetsMaxRadius)
    {
        throw std::runtime_error(rContext + ": 'scan_radius' must be at most "
                                 + std::to_string(k_TableOffsetsMaxRadius));
    }
    config.creditsOffset = ConfigFields::RequireIntAtLeast(rJson, "credits_offset", 0, rContext);
    config.distanceOffset =
        ConfigFields::RequireIntAtLeast(rJson, "distance_offset", 1, rContext);
    config.minPopulation = ConfigFields::RequireIntAtLeast(rJson, "min_population", 0, rContext);
    config.minimum = ConfigFields::RequireIntAtLeast(rJson, "minimum", 0, rContext);
    config.ungarrisonedFactor = RequireFactor_(rJson, "ungarrisoned_factor", rContext);
    config.counterpartyAbsentMultiplier =
        ConfigFields::RequireIntAtLeast(rJson, "counterparty_absent_multiplier", 1, rContext);
    config.loneBaseMultiplier =
        ConfigFields::RequireIntAtLeast(rJson, "lone_base_multiplier", 1, rContext);
    config.outnumberedRatio =
        ConfigFields::RequireIntAtLeast(rJson, "outnumbered_ratio", 1, rContext);
    config.outnumberedFactor = RequireFactor_(rJson, "outnumbered_factor", rContext);
    config.mineralsPerRow =
        ConfigFields::RequireIntAtLeast(rJson, "minerals_per_row", 1, rContext);
    config.projectRowsMultiplier =
        ConfigFields::RequireIntAtLeast(rJson, "project_rows_multiplier", 0, rContext);
    config.unitRowsMultiplier =
        ConfigFields::RequireIntAtLeast(rJson, "unit_rows_multiplier", 0, rContext);
    return config;
}

GiftSizeStepConfig_t ParseGiftSizeStep_(const nlohmann::json& rJson, const std::string& rContext)
{
    ConfigFields::RejectUnknownKeys(rJson,
                                    {"size", "recipient_credits_factor", "giver_credits_factor",
                                     "turn_factor", "minimum"},
                                    rContext);
    GiftSizeStepConfig_t step;
    step.size = RequireGiftSize_(rJson, rContext);
    step.recipientCreditsFactor = RequireFactor_(rJson, "recipient_credits_factor", rContext);
    step.giverCreditsFactor = RequireFactor_(rJson, "giver_credits_factor", rContext);
    step.turnFactor = RequireFactor_(rJson, "turn_factor", rContext);
    step.minimum = ConfigFields::RequireIntAtLeast(rJson, "minimum", 0, rContext);
    return step;
}

// The last rung has no thresholds: it is the size when no other rung is reached.
GiftSizesConfig_t ParseGiftSizes_(const nlohmann::json& rJson, const std::string& rContext)
{
    const nlohmann::json& rSizes = Require_(rJson, "sizes", rContext);
    if (!rSizes.is_array() || rSizes.empty())
    {
        throw std::runtime_error(rContext + ": 'sizes' must be a non-empty array");
    }
    GiftSizesConfig_t sizes;
    for (std::size_t i = 0; i + 1 < rSizes.size(); ++i)
    {
        sizes.steps.push_back(
            ParseGiftSizeStep_(rSizes.at(i), rContext + ".sizes[" + std::to_string(i) + "]"));
    }
    const std::string fallbackContext = rContext + ".sizes (last)";
    ConfigFields::RejectUnknownKeys(rSizes.back(), {"size"}, fallbackContext);
    sizes.fallback = RequireGiftSize_(rSizes.back(), fallbackContext);
    return sizes;
}

CreditFrictionConfig_t ParseCreditFriction_(const nlohmann::json& rJson,
                                            const std::string& rContext)
{
    ConfigFields::RejectUnknownKeys(rJson,
                                    {"credits_per_unit", "first_block_units",
                                     "first_block_divisor", "block_units", "units_per_point",
                                     "factor"},
                                    rContext);
    CreditFrictionConfig_t config;
    config.creditsPerUnit = ConfigFields::RequireIntAtLeast(rJson, "credits_per_unit", 1, rContext);
    config.firstBlockUnits =
        ConfigFields::RequireIntAtLeast(rJson, "first_block_units", 0, rContext);
    config.firstBlockDivisor =
        ConfigFields::RequireIntAtLeast(rJson, "first_block_divisor", 1, rContext);
    config.blockUnits = ConfigFields::RequireIntAtLeast(rJson, "block_units", 1, rContext);
    config.unitsPerPoint = ConfigFields::RequireIntAtLeast(rJson, "units_per_point", 1, rContext);
    config.factor = RequireFactor_(rJson, "factor", rContext);
    return config;
}

// A kind whose friction is its value scaled by one factor.
Rational_t ParseFrictionFactor_(const nlohmann::json& rJson, const std::string& rContext)
{
    const std::string context = rContext + ".friction";
    const nlohmann::json& rFriction = RequireObject_(rJson, "friction", rContext);
    ConfigFields::RejectUnknownKeys(rFriction, {"factor"}, context);
    return RequireFactor_(rFriction, "factor", context);
}

CreditGiftConfig_t ParseCreditGift_(const nlohmann::json& rJson, const std::string& rContext)
{
    ConfigFields::RejectUnknownKeys(rJson, {"sizes", "friction"}, rContext);
    CreditGiftConfig_t config;
    config.sizes = ParseGiftSizes_(rJson, rContext);
    config.friction = ParseCreditFriction_(RequireObject_(rJson, "friction", rContext),
                                           rContext + ".friction");
    return config;
}

TechValueConfig_t ParseTechValue_(const nlohmann::json& rJson, const std::string& rContext)
{
    ConfigFields::RejectUnknownKeys(
        rJson,
        {"priority_factor", "unmatched_factor", "growth_base_count", "extra_owner_penalty",
         "minimum"},
        rContext);
    TechValueConfig_t config;
    config.priorityFactor = ConfigFields::RequireIntAtLeast(rJson, "priority_factor", 0, rContext);
    config.unmatchedFactor = RequireFactor_(rJson, "unmatched_factor", rContext);
    config.growthBaseCount =
        ConfigFields::RequireIntAtLeast(rJson, "growth_base_count", 0, rContext);
    config.extraOwnerPenalty =
        ConfigFields::RequireIntAtLeast(rJson, "extra_owner_penalty", 0, rContext);
    config.minimum = ConfigFields::RequireIntAtLeast(rJson, "minimum", 0, rContext);
    return config;
}

TechGiftConfig_t ParseTechGift_(const nlohmann::json& rJson, const std::string& rContext)
{
    ConfigFields::RejectUnknownKeys(rJson, {"sizes", "friction", "value"}, rContext);
    TechGiftConfig_t config;
    config.sizes = ParseGiftSizes_(rJson, rContext);
    config.frictionFactor = ParseFrictionFactor_(rJson, rContext);
    config.value = ParseTechValue_(RequireObject_(rJson, "value", rContext), rContext + ".value");
    return config;
}

BaseGiftConfig_t ParseBaseGift_(const nlohmann::json& rJson, const std::string& rContext)
{
    ConfigFields::RejectUnknownKeys(rJson, {"sizes", "friction"}, rContext);
    BaseGiftConfig_t config;
    config.sizes = ParseGiftSizes_(rJson, rContext);
    config.frictionFactor = ParseFrictionFactor_(rJson, rContext);
    return config;
}

GiftConfig_t ParseGifts_(const nlohmann::json& rJson, const std::string& rContext)
{
    ConfigFields::RejectUnknownKeys(rJson, {"credits", "technology", "base"}, rContext);
    GiftConfig_t config;
    config.credits =
        ParseCreditGift_(RequireObject_(rJson, "credits", rContext), rContext + ".credits");
    config.technology = ParseTechGift_(RequireObject_(rJson, "technology", rContext),
                                       rContext + ".technology");
    config.base = ParseBaseGift_(RequireObject_(rJson, "base", rContext), rContext + ".base");
    return config;
}

DefensiveObligationMode_t RequireObligationMode_(const nlohmann::json& rJson,
                                                 const std::string& rContext)
{
    const nlohmann::json& rValue = Require_(rJson, "defensive_obligation_mode", rContext);
    if (!rValue.is_string())
    {
        throw std::runtime_error(rContext + ": 'defensive_obligation_mode' must be a string");
    }
    return EnumFromName<DefensiveObligationMode_t>(rValue.get<std::string>(),
                                                   "defensive obligation mode");
}

} // namespace

DiplomacyConfig_t DiplomacyConfigParser::ParseConfig(const std::string& configPath)
{
    std::ifstream file(configPath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open diplomacy config '" + configPath + "'");
    }

    const nlohmann::json json = nlohmann::json::parse(file);
    const std::string context = "Diplomacy config '" + configPath + "'";
    if (!json.is_object())
    {
        throw std::runtime_error(context + " must be a JSON object");
    }
    ConfigFields::RejectUnknownKeys(json,
                                    {"statuses", "defensive_obligation_mode", "friction_max",
                                     "blemishes_max", "break_agreement_blemishes",
                                     "probe_keep_friction_draw", "artifact_capture_friction",
                                     "allied_territory_battle_friction",
                                     "allied_territory_battle_gifts_floor",
                                     "terraform_rainfall_friction",
                                     "terraform_rainfall_stop_distance", "base_value", "gifts"},
                                    context);
    const nlohmann::json& rStatuses = RequireObject_(json, "statuses", context);
    const nlohmann::json& rBaseValue = RequireObject_(json, "base_value", context);
    const nlohmann::json& rGifts = RequireObject_(json, "gifts", context);

    DiplomacyConfig_t config;
    config.defensiveObligationMode = RequireObligationMode_(json, context);
    config.frictionMax = ConfigFields::RequireIntAtLeast(json, "friction_max", 1, context);
    config.blemishesMax = ConfigFields::RequireIntAtLeast(json, "blemishes_max", 1, context);
    config.breakAgreementBlemishes =
        ConfigFields::RequireIntAtLeast(json, "break_agreement_blemishes", 0, context);
    config.probeKeepFrictionDraw =
        ConfigFields::RequireIntAtLeast(json, "probe_keep_friction_draw", 1, context);
    config.artifactCaptureFriction =
        ConfigFields::RequireIntAtLeast(json, "artifact_capture_friction", 0, context);
    config.alliedTerritoryBattleFriction =
        ConfigFields::RequireInt(json, "allied_territory_battle_friction", context);
    config.alliedTerritoryBattleGiftsFloor =
        ConfigFields::RequireIntAtLeast(json, "allied_territory_battle_gifts_floor", 0, context);
    config.terraformRainfallFriction =
        ConfigFields::RequireIntAtLeast(json, "terraform_rainfall_friction", 0, context);
    config.terraformRainfallStopDistance =
        ConfigFields::RequireIntAtLeast(json, "terraform_rainfall_stop_distance", 0, context);
    config.baseValue = ParseBaseValue_(rBaseValue, context + " base_value");
    config.gifts = ParseGifts_(rGifts, context + " gifts");
    config.statuses =
        ConfigFields::ParseEnumKeyedObject<DiplomaticStatus_t, DiplomaticStatusRules_t>(
            rStatuses, "diplomatic status", context,
            [](const nlohmann::json& rEntry, DiplomaticStatus_t status, const std::string& rName) {
                return ParseStatus_(rEntry, status, "statuses." + rName);
            });
    return config;
}

} // namespace ac
