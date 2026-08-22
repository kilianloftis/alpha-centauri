#include "game/GameSettings.h"

#include "lib/config/ConfigFields.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

namespace ac
{

namespace
{

// user_settings.json is hand-editable, so Load is a trust boundary: a value that reaches
// WorldGenerator has already been checked here, and the message names the file and the key
// rather than surfacing from inside generation.
void ValidateMapGeneration_(const MapGenerationConfig_t& rConfig, const std::string& rPath)
{
    const auto fail = [&rPath](const std::string& rMessage) {
        throw std::runtime_error("Game settings '" + rPath + "': map_generation." + rMessage);
    };

    if (rConfig.width <= 0 || rConfig.height <= 0)
    {
        fail("width and height must be positive, got " + std::to_string(rConfig.width) + "x"
             + std::to_string(rConfig.height));
    }
    if (rConfig.oceanCoverage < 0.0f || rConfig.oceanCoverage > 1.0f)
    {
        fail("ocean_coverage must be in [0, 1], got " + std::to_string(rConfig.oceanCoverage));
    }
    if (rConfig.presetId.empty())
    {
        fail("preset_id must not be empty");
    }
}

// A section that is present but not an object is a hand-edit mistake, not an absent section:
// silently substituting defaults is how a user loses their settings without being told.
const nlohmann::json* FindSection_(const nlohmann::json& rJson, const char* name,
                                   const std::string& rPath)
{
    const auto it = rJson.find(name);
    if (it == rJson.end())
    {
        return nullptr;
    }
    if (!it->is_object())
    {
        throw std::runtime_error("Game settings '" + rPath + "': '" + name
                                 + "' must be an object");
    }
    return &*it;
}

void LoadMapGeneration_(const nlohmann::json& rJson, MapGenerationConfig_t& rConfig,
                        const std::string& rPath)
{
    const nlohmann::json* pSection = FindSection_(rJson, "map_generation", rPath);
    if (!pSection)
    {
        return;
    }

    const nlohmann::json& rMap = *pSection;
    rConfig.width = rMap.value("width", rConfig.width);
    rConfig.height = rMap.value("height", rConfig.height);
    rConfig.oceanCoverage = rMap.value("ocean_coverage", rConfig.oceanCoverage);
    if (rMap.contains("erosive_forces"))
    {
        rConfig.erosiveForces = ParseErosiveForces(rMap.at("erosive_forces").get<std::string>());
    }
    rConfig.presetId = rMap.value("preset_id", rConfig.presetId);
    rConfig.seed = rMap.value("seed", rConfig.seed);
}

void LoadGraphics_(const nlohmann::json& rJson, GraphicsConfig_t& rConfig,
                   const std::string& rPath)
{
    const nlohmann::json* pSection = FindSection_(rJson, "graphics", rPath);
    if (!pSection)
    {
        return;
    }

    rConfig.windowWidth = pSection->value("window_width", rConfig.windowWidth);
    rConfig.windowHeight = pSection->value("window_height", rConfig.windowHeight);
    rConfig.windowTitle = pSection->value("window_title", rConfig.windowTitle);
    rConfig.framerateLimit = pSection->value("framerate_limit", rConfig.framerateLimit);
    if (pSection->contains("font_paths"))
    {
        // Replaces the defaults rather than appending: a player naming a font wants that font,
        // and the built-in paths are only a guess at where a distro puts one.
        rConfig.fontPaths =
            ConfigFields::ParseStringArray(*pSection, "font_paths");
    }

    if (rConfig.windowWidth == 0 || rConfig.windowHeight == 0)
    {
        throw std::runtime_error("Game settings '" + rPath
                                 + "': graphics.window_width and window_height must be > 0");
    }
    if (rConfig.fontPaths.empty())
    {
        throw std::runtime_error("Game settings '" + rPath
                                 + "': graphics.font_paths must name at least one font");
    }
}

void LoadGameRules_(const nlohmann::json& rJson, GameRulesConfig_t& rConfig,
                    const std::string& rPath)
{
    if (const nlohmann::json* pSection = FindSection_(rJson, "game_rules", rPath))
    {
        rConfig.pauseAtEndOfTurn =
            pSection->value("pause_at_end_of_turn", rConfig.pauseAtEndOfTurn);
        rConfig.autoReturnLowFuelAir =
            pSection->value("auto_return_low_fuel_air", rConfig.autoReturnLowFuelAir);
        if (pSection->contains("difficulty"))
        {
            if (!pSection->at("difficulty").is_string())
            {
                throw std::runtime_error("Game settings '" + rPath
                                         + "': game_rules.difficulty must be a string");
            }
            rConfig.difficultyId = pSection->at("difficulty").get<std::string>();
        }
    }
}

void LoadVisibility_(const nlohmann::json& rJson, VisibilityConfig_t& rConfig,
                     const std::string& rPath)
{
    if (const nlohmann::json* pSection = FindSection_(rJson, "visibility", rPath))
    {
        rConfig.removeShroud = pSection->value("remove_shroud", rConfig.removeShroud);
        rConfig.removeFog = pSection->value("remove_fog", rConfig.removeFog);
    }
}

void LoadPauseOnEvents_(const nlohmann::json& rJson, PauseOnEventsConfig_t& rConfig,
                        const std::string& rPath)
{
    if (const nlohmann::json* pSection = FindSection_(rJson, "pause_on_events", rPath))
    {
        rConfig.newFacilityBuilt =
            pSection->value("new_facility_built", rConfig.newFacilityBuilt);
        rConfig.nonCombatUnitBuilt =
            pSection->value("non_combat_unit_built", rConfig.nonCombatUnitBuilt);
        rConfig.combatUnitBuilt =
            pSection->value("combat_unit_built", rConfig.combatUnitBuilt);
        rConfig.prototypeBuilt = pSection->value("prototype_built", rConfig.prototypeBuilt);
        rConfig.droneRiots = pSection->value("drone_riots", rConfig.droneRiots);
        rConfig.endOfDroneRiots =
            pSection->value("end_of_drone_riots", rConfig.endOfDroneRiots);
        rConfig.goldenAgeStarts =
            pSection->value("golden_age_starts", rConfig.goldenAgeStarts);
        rConfig.endOfGoldenAge =
            pSection->value("end_of_golden_age", rConfig.endOfGoldenAge);
        rConfig.nutrientLow = pSection->value("nutrient_low", rConfig.nutrientLow);
        rConfig.buildOrdersOutOfDate =
            pSection->value("build_orders_out_of_date", rConfig.buildOrdersOutOfDate);
        rConfig.populationLimitReached =
            pSection->value("population_limit_reached", rConfig.populationLimitReached);
        rConfig.delayInTranscendence =
            pSection->value("delay_in_transcendence", rConfig.delayInTranscendence);
    }
}

nlohmann::json MapGenerationToJson_(const MapGenerationConfig_t& rConfig)
{
    return nlohmann::json{
        {"width", rConfig.width},
        {"height", rConfig.height},
        {"ocean_coverage", rConfig.oceanCoverage},
        {"erosive_forces", ToString(rConfig.erosiveForces)},
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
    m_gameRulesRevision.Bump();
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

void GameSettings::SetPauseOnEvents(const PauseOnEventsConfig_t& rConfig)
{
    if (m_pauseOnEvents == rConfig)
    {
        return;
    }
    m_pauseOnEvents = rConfig;
    OnPauseOnEventsChanged.Emit();
}

void GameSettings::SetPauseAtEndOfTurn(bool value)
{
    GameRulesConfig_t rules = m_gameRules;
    rules.pauseAtEndOfTurn = value;
    SetGameRules(rules);
}

void GameSettings::SetAutoReturnLowFuelAir(bool value)
{
    GameRulesConfig_t rules = m_gameRules;
    rules.autoReturnLowFuelAir = value;
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
    m_path = path;
    std::ifstream file(path);
    if (!file.is_open())
    {
        // First run / missing prefs: keep member defaults.
        return;
    }

    const nlohmann::json json = nlohmann::json::parse(file);

    GameRulesConfig_t gameRules;
    VisibilityConfig_t visibility;
    PauseOnEventsConfig_t pauseOnEvents;
    MapGenerationConfig_t mapGeneration;
    LoadGameRules_(json, gameRules, path);
    LoadVisibility_(json, visibility, path);
    LoadPauseOnEvents_(json, pauseOnEvents, path);
    LoadMapGeneration_(json, mapGeneration, path);
    ValidateMapGeneration_(mapGeneration, path);
    LoadGraphics_(json, m_graphics, path);
    SetGameRules(gameRules);
    SetVisibility(visibility);
    SetPauseOnEvents(pauseOnEvents);
    SetMapGeneration(mapGeneration);
}

void GameSettings::Save() const
{
    Save(m_path);
}

void GameSettings::Save(const std::string& path) const
{
    // One block per config struct, so a new knob has an obvious home and Save/Load do not have
    // to be kept in lockstep by hand.
    nlohmann::json json;
    json["game_rules"] = {
        {"pause_at_end_of_turn", m_gameRules.pauseAtEndOfTurn},
        {"auto_return_low_fuel_air", m_gameRules.autoReturnLowFuelAir},
        {"difficulty", m_gameRules.difficultyId},
    };
    json["visibility"] = {
        {"remove_shroud", m_visibility.removeShroud},
        {"remove_fog", m_visibility.removeFog},
    };
    json["pause_on_events"] = {
        {"new_facility_built", m_pauseOnEvents.newFacilityBuilt},
        {"non_combat_unit_built", m_pauseOnEvents.nonCombatUnitBuilt},
        {"combat_unit_built", m_pauseOnEvents.combatUnitBuilt},
        {"prototype_built", m_pauseOnEvents.prototypeBuilt},
        {"drone_riots", m_pauseOnEvents.droneRiots},
        {"end_of_drone_riots", m_pauseOnEvents.endOfDroneRiots},
        {"golden_age_starts", m_pauseOnEvents.goldenAgeStarts},
        {"end_of_golden_age", m_pauseOnEvents.endOfGoldenAge},
        {"nutrient_low", m_pauseOnEvents.nutrientLow},
        {"build_orders_out_of_date", m_pauseOnEvents.buildOrdersOutOfDate},
        {"population_limit_reached", m_pauseOnEvents.populationLimitReached},
        {"delay_in_transcendence", m_pauseOnEvents.delayInTranscendence},
    };
    json["map_generation"] = MapGenerationToJson_(m_mapGeneration);
    json["graphics"] = {
        {"window_width", m_graphics.windowWidth},
        {"window_height", m_graphics.windowHeight},
        {"window_title", m_graphics.windowTitle},
        {"framerate_limit", m_graphics.framerateLimit},
        {"font_paths", m_graphics.fontPaths},
    };

    std::ofstream file(path);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not write game settings '" + path + "'");
    }
    file << json.dump(2) << '\n';
}

} // namespace ac
