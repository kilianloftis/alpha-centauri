#include "game/economy/CommerceConfigParser.h"

#include "lib/config/JsonConfigLoader.h"

#include <cmath>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

[[noreturn]] void FailCommerce_(const std::string& rConfigPath, const std::string& rMessage)
{
    throw std::runtime_error("Commerce config '" + rConfigPath + "': " + rMessage);
}

double ReadPositiveNumber_(const nlohmann::json& rJson, const char* pKey,
                           const std::string& rConfigPath)
{
    const std::string key = "'" + std::string(pKey) + "'";
    if (!rJson.contains(pKey))
    {
        FailCommerce_(rConfigPath, key + " is required");
    }
    if (!rJson.at(pKey).is_number())
    {
        FailCommerce_(rConfigPath, key + " must be a number");
    }
    const double value = rJson.at(pKey).get<double>();
    if (!std::isfinite(value) || value <= 0.0)
    {
        FailCommerce_(rConfigPath, key + " must be finite and > 0");
    }
    return value;
}

std::string ReadFormula_(const nlohmann::json& rJson, const std::string& rConfigPath)
{
    if (!rJson.contains("formula"))
    {
        FailCommerce_(rConfigPath, "'formula' is required");
    }
    if (!rJson.at("formula").is_string())
    {
        FailCommerce_(rConfigPath, "'formula' must be a string");
    }
    std::string formula = rJson.at("formula").get<std::string>();
    if (formula.empty())
    {
        FailCommerce_(rConfigPath, "'formula' must be a non-empty Lua expression");
    }
    return formula;
}

CommerceConfig_t ParseCommerce_(const nlohmann::json& rJson, const std::string& rConfigPath)
{
    for (const auto& [rKey, rUnused] : rJson.items())
    {
        if (rKey != "pair_multiplier" && rKey != "formula")
        {
            FailCommerce_(rConfigPath, "unknown key '" + rKey + "'");
        }
    }
    std::string formula = ReadFormula_(rJson, rConfigPath);
    CommerceConfig_t config;
    config.pairMultiplier = ReadPositiveNumber_(rJson, "pair_multiplier", rConfigPath);
    config.formula = std::move(formula);
    return config;
}

} // namespace

CommerceConfig_t CommerceConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<CommerceConfig_t>(
        configPath, "commerce", [&configPath](const nlohmann::json& rJson) {
            return ParseCommerce_(rJson, configPath);
        });
}

} // namespace ac
