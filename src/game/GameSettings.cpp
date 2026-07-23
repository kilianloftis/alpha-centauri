#include "game/GameSettings.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

namespace ac
{

namespace
{

void LoadMapGeneration_(const nlohmann::json& rJson, MapGenerationConfig_t& rConfig)
{
    if (!rJson.contains("map_generation") || !rJson["map_generation"].is_object())
    {
        return;
    }

    const nlohmann::json& rMap = rJson["map_generation"];
    rConfig.width = rMap.value("width", rConfig.width);
    rConfig.height = rMap.value("height", rConfig.height);
    rConfig.oceanCoverage = rMap.value("ocean_coverage", rConfig.oceanCoverage);
    rConfig.presetId = rMap.value("preset_id", rConfig.presetId);
    rConfig.seed = rMap.value("seed", rConfig.seed);
}

nlohmann::json MapGenerationToJson_(const MapGenerationConfig_t& rConfig)
{
    return nlohmann::json{
        {"width", rConfig.width},
        {"height", rConfig.height},
        {"ocean_coverage", rConfig.oceanCoverage},
        {"preset_id", rConfig.presetId},
        {"seed", rConfig.seed},
    };
}

} // namespace

void GameSettings::Load(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        // First run / missing prefs: keep member defaults.
        return;
    }

    const nlohmann::json json = nlohmann::json::parse(file);
    m_bPauseAtEndOfTurn = json.value("pause_at_end_of_turn", m_bPauseAtEndOfTurn);
    LoadMapGeneration_(json, m_mapGeneration);
}

void GameSettings::Save(const std::string& path) const
{
    nlohmann::json json;
    json["pause_at_end_of_turn"] = m_bPauseAtEndOfTurn;
    json["map_generation"] = MapGenerationToJson_(m_mapGeneration);

    std::ofstream file(path);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not write game settings '" + path + "'");
    }
    file << json.dump(2) << '\n';
}

} // namespace ac
