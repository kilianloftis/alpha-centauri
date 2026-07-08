#pragma once

#include "game/faction/FactionConfig.h"
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace ac
{

class FactionConfigParser
{
public:
    FactionConfigParser() = default;
    ~FactionConfigParser() = default;

    std::vector<FactionConfig_t> ParseConfig(const std::string& configPath);

private:
    FactionConfig_t ParseFactionDirectory(const std::string& id, const std::string& dirPath);

    static FactionIdentityConfig ParseIdentity(const nlohmann::json& j, const std::string& idFallback);
    static LeaderConfig ParseLeader(const nlohmann::json& j);
    static AITendenciesConfig ParseAITendencies(const nlohmann::json& j);
    static std::vector<std::string> ParseBaseNames(const nlohmann::json& j);
    static std::unordered_map<std::string, std::vector<std::string>> ParsePhrases(
        const nlohmann::json& j);

    static nlohmann::json ReadJsonFile(const std::string& filePath);
    static nlohmann::json ReadRequiredJsonFile(const std::string& filePath);
};

} // namespace ac
