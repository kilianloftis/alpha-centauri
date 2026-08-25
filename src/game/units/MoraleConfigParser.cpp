#include "game/units/MoraleConfigParser.h"

#include "game/effects/EffectConfigParser.h"
#include "game/effects/EffectEnums.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>
#include <string>

using json = nlohmann::json;

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
}

void ValidateMoraleLevelEffect_(const EffectConfig_t& rEffect, int levelIndex)
{
    const auto fail = [&](const std::string& rMessage) {
        throw std::runtime_error("morale_levels.json level " + std::to_string(levelIndex) + ": "
                                 + rMessage);
    };

    const StatModifierEffect_t* pMod = std::get_if<StatModifierEffect_t>(&rEffect.effect);
    if (!pMod)
    {
        fail("only StatModifier effects are allowed");
    }
    if (rEffect.scope != EffectScope_t::ThisUnit)
    {
        fail("effects must use scope ThisUnit");
    }
    if (rEffect.condition.has_value())
    {
        fail("conditions are not supported on morale level effects");
    }
    if (pMod->amountSource.has_value())
    {
        fail("amount_source is not supported on morale level effects");
    }
    if (pMod->selector.has_value())
    {
        fail("tile selectors are not supported on morale level effects");
    }

    switch (pMod->stat)
    {
        case StatId_t::Attack:
        case StatId_t::Defense:
            if (pMod->op != ModifierOp_t::AddPercent)
            {
                fail("attack/defense morale effects must use AddPercent");
            }
            break;
        case StatId_t::PromotionChance:
            switch (pMod->op)
            {
                case ModifierOp_t::MinClamp:
                case ModifierOp_t::MultiplyGeometric:
                case ModifierOp_t::MaxClamp:
                    break;
                default:
                    fail("promotion_chance effects must use MinClamp, MultiplyGeometric, or "
                         "MaxClamp");
            }
            break;
        default:
            fail("stat is not allowed on morale levels (use attack, defense, or "
                 "promotion_chance)");
    }
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

} // namespace

MoraleConfig_t MoraleConfigParser::ParseConfig(const std::string& configPath)
{
    std::ifstream configFile(configPath);
    if (!configFile.is_open())
    {
        throw std::runtime_error("Could not open " + configPath);
    }

    json root;
    configFile >> root;

    MoraleConfig_t config;
    config.baseIntrinsic = root.value("base_intrinsic", 1);
    config.probeBaseIntrinsic = root.value("probe_base_intrinsic", 2);
    config.defenseFloorIndex = root.value("defense_floor_index", 1);
    if (!root.contains("promotion_seed_formula") || !root.at("promotion_seed_formula").is_string())
    {
        throw std::runtime_error(
            "morale_levels.json: 'promotion_seed_formula' must be a non-empty string");
    }
    config.promotionSeedFormula = root.at("promotion_seed_formula").get<std::string>();
    if (config.promotionSeedFormula.empty())
    {
        throw std::runtime_error(
            "morale_levels.json: 'promotion_seed_formula' must be a non-empty string");
    }

    if (!root.contains("levels") || !root.at("levels").is_array())
    {
        throw std::runtime_error("morale_levels.json: expected 'levels' array");
    }

    for (const json& rLevelJson : root.at("levels"))
    {
        MoraleLevel_t level;
        level.index = rLevelJson.at("index").get<int>();
        level.conventional = rLevelJson.at("conventional").get<std::string>();
        level.native = rLevelJson.at("native").get<std::string>();
        const std::string sourceId = "morale_level_" + std::to_string(level.index);
        level.effects = EffectConfigParser::ParseEffects(
            rLevelJson, EffectSourceKind_t::MoraleLevel, sourceId);
        config.levels.push_back(std::move(level));
    }

    ValidateContiguousLevels_(config);
    ValidateMoraleLevelEffects_(config);
    return config;
}

} // namespace ac
