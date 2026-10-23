#include "game/production/ProductionConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "game/production/ScrapConfigParser.h"
#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"
#include "game/production/ConstructableKind.h"
#include "lib/config/JsonConfigLoader.h"
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

namespace
{

[[noreturn]] void FailProduction_(const std::string& rConfigPath, const std::string& rMessage)
{
    throw std::runtime_error("Production config '" + rConfigPath + "': " + rMessage);
}

// rWhere names where the keys sit: "" at the top level, else a kind or hurry path.
std::string ProductionContext_(const std::string& rConfigPath, const std::string& rWhere)
{
    const std::string file = "Production config '" + rConfigPath + "'";
    return rWhere.empty() ? file : file + ": " + rWhere;
}

int ReadInRange_(const nlohmann::json& rJson, const char* pKey, int minimum,
                 std::optional<int> maximum, const std::string& rConfigPath)
{
    const std::string key = "'" + std::string(pKey) + "'";
    if (!rJson.contains(pKey))
    {
        FailProduction_(rConfigPath, key + " is required");
    }
    if (!rJson.at(pKey).is_number_integer())
    {
        FailProduction_(rConfigPath, key + " must be an integer");
    }
    const int value = rJson.at(pKey).get<int>();
    if (value < minimum)
    {
        FailProduction_(rConfigPath, key + " must be >= " + std::to_string(minimum) + ", got "
                                         + std::to_string(value));
    }
    if (maximum && value > *maximum)
    {
        FailProduction_(rConfigPath, key + " must be <= " + std::to_string(*maximum) + ", got "
                                         + std::to_string(value));
    }
    return value;
}

int ReadHurryInt_(const nlohmann::json& rHurry, const char* pKey, int minimum,
                  const std::string& rHurryPath, const std::string& rConfigPath)
{
    if (!rHurry.contains(pKey) || !rHurry.at(pKey).is_number_integer())
    {
        FailProduction_(rConfigPath, rHurryPath + std::string(pKey) + " must be an integer");
    }
    const int value = rHurry.at(pKey).get<int>();
    if (value < minimum)
    {
        FailProduction_(rConfigPath, rHurryPath + std::string(pKey) + " must be >= "
                                         + std::to_string(minimum) + ", got "
                                         + std::to_string(value));
    }
    return value;
}

HurryKindConfig_t ParseHurry_(const nlohmann::json& rValue, const std::string& rKindPath,
                              const std::string& rConfigPath)
{
    const std::string hurryPath = rKindPath + ".hurry ";
    if (!rValue.is_object())
    {
        FailProduction_(rConfigPath, hurryPath + "must be an object with formula, "
                                     "mineral_threshold, and below_threshold_multiplier");
    }
    ConfigFields::RejectUnknownKeys(
        rValue, {"formula", "mineral_threshold", "below_threshold_multiplier"},
        ProductionContext_(rConfigPath, rKindPath + ".hurry"));
    if (!rValue.contains("formula") || !rValue.at("formula").is_string()
        || rValue.at("formula").get<std::string>().empty())
    {
        FailProduction_(rConfigPath, hurryPath + "formula must be a non-empty string");
    }
    HurryKindConfig_t kind;
    kind.formula = rValue.at("formula").get<std::string>();
    kind.mineralThreshold = ReadHurryInt_(rValue, "mineral_threshold", 0, hurryPath, rConfigPath);
    kind.belowThresholdMultiplier =
        ReadHurryInt_(rValue, "below_threshold_multiplier", 1, hurryPath, rConfigPath);
    return kind;
}

// Stockpile is rejected, since it never completes.
ConstructableKind_t ParseKindId_(const std::string& rKind, const std::string& rKindPath,
                                 const std::string& rConfigPath)
{
    if (rKind.empty())
    {
        FailProduction_(rConfigPath, "kinds: kind must be a non-empty string");
    }
    ConstructableKind_t kindId = ConstructableKind_t::Building;
    try
    {
        kindId = ParseConstructableKind(rKind);
    }
    catch (const std::runtime_error&)
    {
        FailProduction_(rConfigPath, rKindPath + " is not a known constructable kind");
    }
    if (kindId == ConstructableKind_t::Stockpile)
    {
        FailProduction_(rConfigPath,
                        rKindPath + " cannot be hurried or scrapped; omit it from kinds");
    }
    return kindId;
}

ScrapKindConfig_t ParseDefaultScrap_(ConstructableKind_t kindId, const nlohmann::json& rValue,
                                     const std::string& rKindPath, const std::string& rConfigPath)
{
    if (kindId == ConstructableKind_t::SecretProject)
    {
        FailProduction_(rConfigPath, rKindPath + ".default_scrap cannot be configured; secret "
                                                 "projects cannot be scrapped");
    }
    try
    {
        return ScrapConfigParser::ParseKindConfig(rValue, rKindPath + ".default_scrap");
    }
    catch (const std::runtime_error& rError)
    {
        FailProduction_(rConfigPath, rError.what());
    }
}

ProductionKindConfig_t ParseKind_(ConstructableKind_t kindId, const nlohmann::json& rValue,
                                  const std::string& rKindPath, const std::string& rConfigPath)
{
    if (!rValue.is_object())
    {
        FailProduction_(rConfigPath,
                        rKindPath + " must be an object with hurry and/or default_scrap");
    }
    ConfigFields::RejectUnknownKeys(rValue, {"hurry", "default_scrap"},
                                    ProductionContext_(rConfigPath, rKindPath));
    ProductionKindConfig_t kindConfig;
    if (rValue.contains("hurry"))
    {
        kindConfig.hurry = ParseHurry_(rValue.at("hurry"), rKindPath, rConfigPath);
    }
    if (rValue.contains("default_scrap"))
    {
        kindConfig.defaultScrap =
            ParseDefaultScrap_(kindId, rValue.at("default_scrap"), rKindPath, rConfigPath);
    }
    if (!kindConfig.hurry && !kindConfig.defaultScrap)
    {
        FailProduction_(rConfigPath, rKindPath + " must include hurry or default_scrap");
    }
    return kindConfig;
}

// An empty object turns hurry and scrap off.
decltype(ProductionConfig_t::kinds) ParseKinds_(const nlohmann::json& rJson,
                                                const std::string& rConfigPath)
{
    if (!rJson.contains("kinds") || !rJson.at("kinds").is_object())
    {
        FailProduction_(rConfigPath, "'kinds' object is required");
    }
    decltype(ProductionConfig_t::kinds) kinds;
    for (const auto& [rKind, rValue] : rJson.at("kinds").items())
    {
        const std::string kindPath = "kinds." + rKind;
        const ConstructableKind_t kindId = ParseKindId_(rKind, kindPath, rConfigPath);
        kinds.emplace(kindId, ParseKind_(kindId, rValue, kindPath, rConfigPath));
    }
    return kinds;
}

ProductionConfig_t ParseProduction_(const nlohmann::json& rJson, const std::string& rConfigPath)
{
    ConfigFields::RejectUnknownKeys(rJson,
                       {"retool_penalty_threshold", "retool_penalty_percent",
                        "prototype_surcharge_percent", "kinds", "effects",
                        "on_unit_produced_effects"},
                       ProductionContext_(rConfigPath, ""));
    ProductionConfig_t config;
    config.retoolPenaltyThreshold =
        ReadInRange_(rJson, "retool_penalty_threshold", 0, std::nullopt, rConfigPath);
    config.retoolPenaltyPercent =
        ReadInRange_(rJson, "retool_penalty_percent", 0, 100, rConfigPath);
    // No upper bound: a prototype that costs triple is a coherent mod
    // choice, unlike a retool penalty over 100%.
    config.prototypeSurchargePercent =
        ReadInRange_(rJson, "prototype_surcharge_percent", 0, std::nullopt, rConfigPath);
    config.kinds = ParseKinds_(rJson, rConfigPath);
    config.effects = EffectConfigParser::ParseEffects(
        rJson, EffectSourceKind_t::Production, "production");
    config.onUnitProducedEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rJson, "on_unit_produced_effects", "production");
    return config;
}

} // namespace

ProductionConfig_t ProductionConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<ProductionConfig_t>(
        configPath, "production", [&configPath](const nlohmann::json& rJson) {
            return ParseProduction_(rJson, configPath);
        });
}

} // namespace ac
