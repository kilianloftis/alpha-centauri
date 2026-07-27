#include "game/units/MoraleConfigParser.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

namespace ac
{

namespace
{

PromotionFormula_t ParsePromotionFormula_(const std::string& rFormula)
{
    if (rFormula == "defense_over_total")
    {
        return PromotionFormula_t::DefenseOverTotal;
    }
    if (rFormula == "defense_over_total_half")
    {
        return PromotionFormula_t::DefenseOverTotalHalf;
    }
    throw std::runtime_error("Unknown promotion formula: '" + rFormula + "'");
}

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
        level.combatBonusPercent = rLevelJson.at("combat_bonus_percent").get<double>();
        config.levels.push_back(std::move(level));
    }

    if (root.contains("promotion") && root.at("promotion").contains("rules"))
    {
        for (const json& rRuleJson : root.at("promotion").at("rules"))
        {
            PromotionRule_t rule;
            if (rRuleJson.contains("level"))
            {
                rule.minLevel = rRuleJson.at("level").get<int>();
                rule.maxLevel = rule.minLevel;
            }
            else
            {
                rule.minLevel = rRuleJson.value("min_level", 0);
                rule.maxLevel = rRuleJson.value("max_level", rule.minLevel);
            }
            if (rRuleJson.contains("chance"))
            {
                rule.formula = PromotionFormula_t::FlatChance;
                rule.chance = rRuleJson.at("chance").get<double>();
            }
            else if (rRuleJson.contains("formula"))
            {
                rule.formula = ParsePromotionFormula_(rRuleJson.at("formula").get<std::string>());
            }
            else
            {
                throw std::runtime_error(
                    "morale_levels.json: promotion rule needs 'chance' or 'formula'");
            }
            if (rule.minLevel > rule.maxLevel)
            {
                throw std::runtime_error("morale_levels.json: promotion min_level > max_level");
            }
            config.promotionRules.push_back(rule);
        }
    }

    ValidateContiguousLevels_(config);
    return config;
}

} // namespace ac
