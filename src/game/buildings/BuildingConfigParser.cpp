#include "game/buildings/BuildingConfigParser.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

using json = nlohmann::json;

namespace ac
{

namespace
{

StatId ParseStatId(const std::string& statStr)
{
    if (statStr == "Nutrients" || statStr == "nutrients") return StatId::Nutrients;
    if (statStr == "Minerals" || statStr == "minerals") return StatId::Minerals;
    if (statStr == "Energy" || statStr == "energy") return StatId::Energy;
    throw std::runtime_error("Unknown stat id: '" + statStr + "'");
}

} // namespace

BuildingConfigParser::BuildingConfigParser()
{
}

std::vector<BuildingConfig_t> BuildingConfigParser::ParseConfig(const std::string& configPath)
{
    if (fs::is_directory(configPath))
    {
        std::vector<fs::path> files;
        for (const auto& rEntry : fs::directory_iterator(configPath))
        {
            if (rEntry.path().extension() == ".json")
                files.push_back(rEntry.path());
        }
        std::sort(files.begin(), files.end());

        std::vector<BuildingConfig_t> all;
        for (const auto& rFile : files)
        {
            auto configs = ParseFile_(rFile.string());
            all.insert(all.end(), configs.begin(), configs.end());
        }
        return all;
    }

    return ParseFile_(configPath);
}

std::vector<BuildingConfig_t> BuildingConfigParser::ParseFile_(const std::string& filePath)
{
    std::cout << "Loading building configuration from: " << filePath << "\n";

    std::ifstream configFile(filePath);
    if (!configFile.is_open())
    {
        throw std::runtime_error("Could not open " + filePath);
    }

    json configJson;
    configFile >> configJson;

    if (!configJson.is_array())
    {
        throw std::runtime_error("Expected array of buildings in '" + filePath + "'");
    }

    std::vector<BuildingConfig_t> configs;
    for (const auto& buildingJson : configJson)
    {
        configs.push_back(ParseBuildingConfig(buildingJson));
    }

    std::cout << "Loaded " << configs.size() << " building configurations\n";
    return configs;
}

BuildingConfig_t BuildingConfigParser::ParseBuildingConfig(const nlohmann::json& buildingJson)
{
    BuildingConfig_t config;
    config.id = buildingJson["id"];
    config.name = buildingJson.value("name", config.id);
    config.nutrientsBonus = buildingJson.value("nutrients_bonus", 0);
    config.mineralCost = buildingJson.value("mineral_cost", 0);
    config.allowMultiple = buildingJson.value("allow_multiple", false);
    config.bIsSecretProject = buildingJson.value("secret_project", false);
    
    if (buildingJson.contains("required_techs"))
    {
        for (const auto& tech : buildingJson["required_techs"])
        {
            config.requiredTechs.push_back(tech);
        }
    }

    if (buildingJson.contains("improvement_bonuses"))
    {
        for (const auto& [key, val] : buildingJson["improvement_bonuses"].items())
        {
            BuildingImprovementBonus_t bonus;
            bonus.nutrients = val.value("nutrients", 0);
            config.improvementBonuses[key] = bonus;
        }
    }

    if (buildingJson.contains("effects"))
    {
        for (const auto& effectJson : buildingJson["effects"])
        {
            config.effects.push_back(ParseBuildingEffect(effectJson));
        }
    }

    return config;
}

EffectConfig_t BuildingConfigParser::ParseBuildingEffect(const nlohmann::json& effectJson)
{
    EffectConfig_t effect;

    const std::string typeStr = effectJson["type"];
    const std::string scopeStr = effectJson["scope"];
    const std::string persistenceStr = effectJson.value("persistence", "Continuous");
    const auto& parameters = effectJson.value("parameters", nlohmann::json::object());

    if (scopeStr == "ThisBase")           effect.scope = EffectScope_t::ThisBase;
    else if (scopeStr == "AllOwnerBases") effect.scope = EffectScope_t::AllOwnerBases;
    else if (scopeStr == "FactionUnits")  effect.scope = EffectScope_t::FactionUnits;
    else if (scopeStr == "FactionGlobal") effect.scope = EffectScope_t::FactionGlobal;
    else if (scopeStr == "WorldGlobal")   effect.scope = EffectScope_t::WorldGlobal;
    else throw std::runtime_error("Unknown building effect scope: '" + scopeStr + "'");

    if (persistenceStr == "Instantaneous")   effect.persistence = EffectPersistence_t::Instantaneous;
    else if (persistenceStr == "Continuous") effect.persistence = EffectPersistence_t::Continuous;
    else throw std::runtime_error("Unknown building effect persistence: '" + persistenceStr + "'");

    effect.condition = effectJson.value("condition", "");

    if (typeStr == "GrantBuilding")
    {
        GrantBuildingEffect_t grantBuilding;
        grantBuilding.buildingId = parameters.value("building_id", "");
        effect.effect = grantBuilding;
    }
    else if (typeStr == "GrantTech")
    {
        GrantTechEffect_t grantTech;
        grantTech.techId = parameters.value("tech_id", "");
        effect.effect = grantTech;
    }
    else if (typeStr == "GrantUnit")
    {
        GrantUnitEffect_t grantUnit;
        grantUnit.unitId = parameters.value("unit_id", "");
        effect.effect = grantUnit;
    }
    else if (typeStr == "StatModifier")
    {
        StatModifierEffect_t statModifier;
        statModifier.stat = ParseStatId(parameters.value("stat", ""));

        const auto& amountJson = parameters.value("amount", nlohmann::json(0.0));
        if (amountJson.is_number())
        {
            statModifier.amount = amountJson.get<double>();
        }
        else if (amountJson.is_string())
        {
            statModifier.amount = std::stod(amountJson.get<std::string>());
        }
        else
        {
            statModifier.amount = 0.0;
        }

        const std::string opStr = parameters.value("op", "Add");
        if (opStr == "Add") statModifier.op = ModifierOp::Add;
        else if (opStr == "MultiplyGeometric") statModifier.op = ModifierOp::MultiplyGeometric;
        else if (opStr == "MultiplyArithmetic") statModifier.op = ModifierOp::MultiplyArithmetic;
        else throw std::runtime_error("Unknown stat modifier op: '" + opStr + "'");

        effect.effect = statModifier;
    }
    else if (typeStr == "RuleFlag")
    {
        RuleFlagEffect_t ruleFlag;
        ruleFlag.flag = parameters.value("flag", "");
        effect.effect = ruleFlag;
    }
    else if (typeStr == "SocialEngineeringOverride")
    {
        // TODO: define parsing when social engineering rules are finalized
        SocialEngineeringOverrideEffect_t override;
        override.category = parameters.value("category", "");
        override.choice = parameters.value("choice", "");
        effect.effect = override;
    }
    else if (typeStr == "DiplomaticModifier")
    {
        // TODO: define parsing when diplomatic modifier rules are finalized
        DiplomaticModifierEffect_t diplomatic;
        diplomatic.targetFactionId = parameters.value("target_faction_id", "");
        diplomatic.value = std::stoi(parameters.value("value", "0"));
        effect.effect = diplomatic;
    }
    else if (typeStr == "TileYieldModifier")
    {
        TileYieldModifierEffect_t tileModifier;

        tileModifier.resource = ParseStatId(parameters.value("resource", "Nutrients"));

        const auto& selectorJson = parameters.value("selector", nlohmann::json::object());
        const std::string selectorKindStr = selectorJson.value("kind", "BaseTile");
        if (selectorKindStr == "BaseTile")
        {
            tileModifier.selector.kind = TileSelectorKind::BaseTile;
        }
        else if (selectorKindStr == "HasImprovement")
        {
            tileModifier.selector.kind = TileSelectorKind::HasImprovement;
            const std::string improvementStr = selectorJson.value("improvement", "");
            if (improvementStr == "Farm") tileModifier.selector.improvement = ImprovementType::Farm;
            else if (improvementStr == "Condenser") tileModifier.selector.improvement = ImprovementType::Condenser;
            else throw std::runtime_error("Unknown tile improvement type: '" + improvementStr + "'");
        }
        else throw std::runtime_error("Unknown tile selector kind: '" + selectorKindStr + "'");

        const auto& amountJson = parameters.value("amount", nlohmann::json(0.0));
        if (amountJson.is_number())
        {
            tileModifier.amount = amountJson.get<double>();
        }
        else if (amountJson.is_string())
        {
            tileModifier.amount = std::stod(amountJson.get<std::string>());
        }
        else
        {
            tileModifier.amount = 0.0;
        }

        const std::string opStr = parameters.value("op", "Add");
        if (opStr == "Add") tileModifier.op = ModifierOp::Add;
        else if (opStr == "MultiplyGeometric") tileModifier.op = ModifierOp::MultiplyGeometric;
        else if (opStr == "MultiplyArithmetic") tileModifier.op = ModifierOp::MultiplyArithmetic;
        else throw std::runtime_error("Unknown tile yield modifier op: '" + opStr + "'");

        effect.effect = tileModifier;
    }
    else throw std::runtime_error("Unknown building effect type: '" + typeStr + "'");

    return effect;
}

} // namespace ac
