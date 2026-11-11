#include "game/combat/MoraleConfigParser.h"

#include "game/effects/EffectConfigParser.h"
#include "game/effects/EffectEnums.h"
#include "lib/config/JsonConfigLoader.h"

#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

void ValidateContiguousLevels_(const MoraleConfig_t& rConfig)
{
    if (rConfig.levels.empty())
    {
        throw std::runtime_error("morale_levels.json: levels must be non-empty");
    }
    for (size_t i = 0; i < rConfig.levels.size(); ++i)
    {
        if (rConfig.levels[i].index != static_cast<int>(i))
        {
            throw std::runtime_error(
                "morale_levels.json: levels must be contiguous indices starting at 0");
        }
        if (rConfig.levels[i].conventional.empty() || rConfig.levels[i].native.empty())
        {
            throw std::runtime_error(
                "morale_levels.json: each level needs non-empty conventional and native names");
        }
    }
    if (rConfig.defenseFloorIndex < rConfig.MinLevel()
        || rConfig.defenseFloorIndex > rConfig.MaxLevel())
    {
        throw std::runtime_error("morale_levels.json: defense_floor_index out of level range");
    }
    if (rConfig.FindLevel(rConfig.baselineIndex) == nullptr)
    {
        throw std::runtime_error("morale_levels.json: baseline_index out of level range");
    }
}

[[noreturn]] void FailLevel_(int levelIndex, const std::string& rMessage)
{
    throw std::runtime_error("morale_levels.json level " + std::to_string(levelIndex) + ": "
                             + rMessage);
}

// A plain unconditional ThisUnit StatModifier, the only shape a morale level may carry.
const StatModifierEffect_t& RequireMoraleEffectShape_(const EffectConfig_t& rEffect,
                                                      int levelIndex)
{
    const StatModifierEffect_t* pMod = std::get_if<StatModifierEffect_t>(&rEffect.effect);
    if (!pMod)
    {
        FailLevel_(levelIndex, "only StatModifier effects are allowed");
    }
    if (rEffect.scope != EffectScope_t::ThisUnit)
    {
        FailLevel_(levelIndex, "effects must use scope ThisUnit");
    }
    if (rEffect.condition.has_value())
    {
        FailLevel_(levelIndex, "conditions are not supported on morale level effects");
    }
    if (pMod->amountSource.has_value())
    {
        FailLevel_(levelIndex, "amount_source is not supported on morale level effects");
    }
    if (pMod->selector.has_value())
    {
        FailLevel_(levelIndex, "tile selectors are not supported on morale level effects");
    }
    return *pMod;
}

void ValidateMoraleStatOp_(const StatModifierEffect_t& rMod, int levelIndex)
{
    switch (rMod.stat)
    {
        case StatId_t::Attack:
        case StatId_t::Defense:
            FailLevel_(levelIndex,
                       "attack/defense combat scale is percent_per_level, not a level effect");
        case StatId_t::PromotionEligibilityChance:
        case StatId_t::PromotionSuccessChance:
            switch (rMod.op)
            {
                case ModifierOp_t::MinClamp:
                case ModifierOp_t::MultiplyGeometric:
                case ModifierOp_t::MaxClamp:
                    return;
                default:
                    FailLevel_(levelIndex, "promotion eligibility/success effects must use "
                                           "MinClamp, MultiplyGeometric, or MaxClamp");
            }
        case StatId_t::PlanetPearls:
            if (rMod.op != ModifierOp_t::MultiplyGeometric)
            {
                FailLevel_(levelIndex, "planet_pearls morale effects must use MultiplyGeometric");
            }
            return;
        default:
            FailLevel_(levelIndex,
                       "stat is not allowed on morale levels (use promotion_eligibility_chance, "
                       "promotion_success_chance, or planet_pearls)");
    }
}

void ValidateMoraleLevelEffect_(const EffectConfig_t& rEffect, int levelIndex)
{
    const StatModifierEffect_t& rMod = RequireMoraleEffectShape_(rEffect, levelIndex);
    ValidateMoraleStatOp_(rMod, levelIndex);
}

void ValidateMoraleLevelEffects_(const MoraleConfig_t& rConfig)
{
    for (const MoraleLevel_t& rLevel : rConfig.levels)
    {
        for (const EffectConfig_t& rEffect : rLevel.effects)
        {
            ValidateMoraleLevelEffect_(rEffect, rLevel.index);
        }
    }
}

std::vector<MoraleLevel_t> ParseLevels_(const ConfigReader& rReader)
{
    std::vector<MoraleLevel_t> levels;
    for (const ConfigReader& rLevelReader : rReader.Objects("levels"))
    {
        MoraleLevel_t level;
        level.index = rLevelReader.Int("index");
        level.conventional = rLevelReader.String("conventional");
        level.native = rLevelReader.String("native");
        const std::string sourceId = "morale_level_" + std::to_string(level.index);
        level.effects = EffectConfigParser::ParseEffects(
            rLevelReader.Json(), EffectSourceKind_t::MoraleLevel, sourceId);
        levels.push_back(std::move(level));
    }
    return levels;
}

MoraleConfig_t ParseMorale_(const ConfigReader& rReader)
{
    MoraleConfig_t config;
    config.startingIntrinsic = rReader.IntOr("starting_intrinsic", 1);
    config.defenseFloorIndex = rReader.IntOr("defense_floor_index", 1);
    config.percentPerLevel = rReader.Number("percent_per_level");
    config.baselineIndex = rReader.Int("baseline_index");
    config.promotionEligibilityFormula = rReader.NonEmptyString("promotion_eligibility_formula");
    config.levels = ParseLevels_(rReader);

    ValidateContiguousLevels_(config);
    ValidateMoraleLevelEffects_(config);
    return config;
}

} // namespace

MoraleConfig_t MoraleConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<MoraleConfig_t>(configPath, "morale levels",
                                                            ParseMorale_);
}

} // namespace ac
