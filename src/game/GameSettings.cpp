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

void LoadGameRules_(const nlohmann::json& rJson, GameRulesConfig_t& rConfig)
{
    if (rJson.contains("game_rules") && rJson["game_rules"].is_object())
    {
        const nlohmann::json& rRules = rJson["game_rules"];
        rConfig.pauseAtEndOfTurn = rRules.value("pause_at_end_of_turn", rConfig.pauseAtEndOfTurn);
        return;
    }

    // Backward compatibility: older prefs stored pause at the top level.
    rConfig.pauseAtEndOfTurn = rJson.value("pause_at_end_of_turn", rConfig.pauseAtEndOfTurn);
}

void LoadVisibility_(const nlohmann::json& rJson, VisibilityConfig_t& rConfig)
{
    // Prefs keep remove_shroud under game_rules and remove_fog under debug_options so
    // existing user_settings.json files continue to load.
    if (rJson.contains("game_rules") && rJson["game_rules"].is_object())
    {
        rConfig.removeShroud = rJson["game_rules"].value("remove_shroud", rConfig.removeShroud);
    }
    if (rJson.contains("debug_options") && rJson["debug_options"].is_object())
    {
        rConfig.removeFog = rJson["debug_options"].value("remove_fog", rConfig.removeFog);
    }
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

void GameSettings::SetGameRules(const GameRulesConfig_t& rConfig)
{
    if (m_gameRules == rConfig)
    {
        return;
    }
    m_gameRules = rConfig;
    OnGameRulesChanged.Emit();
}

void GameSettings::SetVisibility(const VisibilityConfig_t& rConfig)
{
    if (m_visibility == rConfig)
    {
        return;
    }
    m_visibility = rConfig;
    OnVisibilityChanged.Emit();
}

void GameSettings::SetPauseAtEndOfTurn(bool value)
{
    GameRulesConfig_t rules = m_gameRules;
    rules.pauseAtEndOfTurn = value;
    SetGameRules(rules);
}

void GameSettings::SetMapGeneration(const MapGenerationConfig_t& rConfig)
{
    if (m_mapGeneration == rConfig)
    {
        return;
    }
    m_mapGeneration = rConfig;
    OnMapGenerationChanged.Emit();
}

void GameSettings::Load(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        // First run / missing prefs: keep member defaults.
        return;
    }

    const nlohmann::json json = nlohmann::json::parse(file);

    GameRulesConfig_t gameRules;
    VisibilityConfig_t visibility;
    MapGenerationConfig_t mapGeneration;
    LoadGameRules_(json, gameRules);
    LoadVisibility_(json, visibility);
    LoadMapGeneration_(json, mapGeneration);
    SetGameRules(gameRules);
    SetVisibility(visibility);
    SetMapGeneration(mapGeneration);
}

void GameSettings::Save(const std::string& path) const
{
    nlohmann::json json;
    json["game_rules"] = {
        {"pause_at_end_of_turn", m_gameRules.pauseAtEndOfTurn},
        {"remove_shroud", m_visibility.removeShroud},
    };
    json["debug_options"] = {
        {"remove_fog", m_visibility.removeFog},
    };
    json["map_generation"] = MapGenerationToJson_(m_mapGeneration);

    std::ofstream file(path);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not write game settings '" + path + "'");
    }
    file << json.dump(2) << '\n';
}

} // namespace ac
