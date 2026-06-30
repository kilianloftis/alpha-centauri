#include "game/units/UnitComponentConfigParser.h"
#include "lib/effects/BonusEffect.h"
#include "lib/effects/EffectEnums.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace ac
{

namespace
{

StatId ParseUnitStatId_(const std::string& rStat)
{
    if (rStat == "attack")               return StatId::Attack;
    if (rStat == "defense")              return StatId::Defense;
    if (rStat == "movement")             return StatId::Movement;
    if (rStat == "hit_points")           return StatId::HitPoints;
    if (rStat == "disengage_chance")     return StatId::DisengageChance;
    if (rStat == "fuel")                 return StatId::Fuel;
    if (rStat == "damage_from_out_of_fuel") return StatId::DamageFromOutOfFuel;
    if (rStat == "cargo_capacity")       return StatId::CargoCapacity;
    if (rStat == "difficult_terrain_cost") return StatId::DifficultTerrainCost;
    if (rStat == "cost_multiplier")      return StatId::CostMultiplier;
    throw std::runtime_error("Unknown unit stat id: '" + rStat + "'");
}

RuleFlagId ParseRuleFlagId_(const std::string& rFlag)
{
    if (rFlag == "flight")      return RuleFlagId::Flight;
    if (rFlag == "single_use")  return RuleFlagId::SingleUse;
    throw std::runtime_error("Unknown rule flag id: '" + rFlag + "'");
}

} // namespace

std::vector<UnitComponentConfig_t> UnitComponentConfigParser::ParseConfig(const std::string& rConfigPath)
{
    if (fs::is_directory(rConfigPath))
    {
        std::vector<fs::path> files;
        for (const auto& rEntry : fs::directory_iterator(rConfigPath))
        {
            if (rEntry.path().extension() == ".json")
                files.push_back(rEntry.path());
        }
        std::sort(files.begin(), files.end());

        std::vector<UnitComponentConfig_t> all;
        for (const auto& rFile : files)
        {
            auto configs = ParseFile_(rFile.string());
            all.insert(all.end(), configs.begin(), configs.end());
        }
        return all;
    }

    return ParseFile_(rConfigPath);
}

std::vector<UnitComponentConfig_t> UnitComponentConfigParser::ParseFile_(const std::string& rFilePath)
{
    std::cout << "Loading unit component configuration from: " << rFilePath << "\n";

    std::ifstream configFile(rFilePath);
    if (!configFile.is_open())
    {
        throw std::runtime_error("Could not open " + rFilePath);
    }

    json configJson;
    configFile >> configJson;

    if (!configJson.is_array())
    {
        throw std::runtime_error("Expected array of unit components in '" + rFilePath + "'");
    }

    std::vector<UnitComponentConfig_t> configs;
    for (const auto& rComponentJson : configJson)
    {
        configs.push_back(ParseComponentConfig(rComponentJson));
    }

    std::cout << "Loaded " << configs.size() << " unit component configurations\n";
    return configs;
}

UnitComponentConfig_t UnitComponentConfigParser::ParseComponentConfig(const nlohmann::json& rComponentJson)
{
    UnitComponentConfig_t config;
    config.id = rComponentJson["id"];
    config.name = rComponentJson.value("name", config.id);
    config.type = rComponentJson["type"].get<std::string>();
    config.requiredTech = rComponentJson.value("required_tech", std::string(""));
    config.mineralCost = rComponentJson.value("mineral_cost", 0);

    if (rComponentJson.contains("stats"))
    {
        for (const auto& [key, val] : rComponentJson["stats"].items())
        {
            const StatId statId = ParseUnitStatId_(key);
            const double base = val.value("base", 0.0);
            const double additiveMult = val.value("additive_mult", 0.0);
            const double geometricMult = val.value("geometric_mult", 1.0);

            if (base != 0.0)
            {
                EffectConfig_t effect;
                effect.effect = StatModifierEffect_t{statId, base, ModifierOp::Add};
                effect.scope = EffectScope_t::ThisUnit;
                effect.persistence = EffectPersistence_t::Continuous;
                config.effects.push_back(effect);
            }
            if (additiveMult != 0.0)
            {
                EffectConfig_t effect;
                effect.effect = StatModifierEffect_t{statId, 1.0 + additiveMult, ModifierOp::MultiplyArithmetic};
                effect.scope = EffectScope_t::ThisUnit;
                effect.persistence = EffectPersistence_t::Continuous;
                config.effects.push_back(effect);
            }
            if (geometricMult != 1.0)
            {
                EffectConfig_t effect;
                effect.effect = StatModifierEffect_t{statId, geometricMult, ModifierOp::MultiplyGeometric};
                effect.scope = EffectScope_t::ThisUnit;
                effect.persistence = EffectPersistence_t::Continuous;
                config.effects.push_back(effect);
            }
        }
    }

    if (rComponentJson.contains("flags"))
    {
        for (const auto& [key, val] : rComponentJson["flags"].items())
        {
            if (val.get<bool>())
            {
                EffectConfig_t effect;
                effect.effect = RuleFlagEffect_t{ParseRuleFlagId_(key)};
                effect.scope = EffectScope_t::ThisUnit;
                effect.persistence = EffectPersistence_t::Continuous;
                config.effects.push_back(effect);
            }
        }
    }

    if (rComponentJson.contains("bonus_tables"))
    {
        for (const auto& [tableName, tableVal] : rComponentJson["bonus_tables"].items())
        {
            for (const auto& [key, val] : tableVal.items())
            {
                EffectConfig_t effect;
                effect.effect = UnitBonusTableEffect_t{tableName, key, val.get<float>()};
                effect.scope = EffectScope_t::ThisUnit;
                effect.persistence = EffectPersistence_t::Continuous;
                config.effects.push_back(effect);
            }
        }
    }

    return config;
}

} // namespace ac
