#pragma once

#include "game/DebugOptionsConfig.h"
#include "game/GameRulesConfig.h"
#include "game/map/MapGenerationConfig.h"
#include <string>

namespace ac
{

// Cross-save player preferences (not part of GameState / save-game serialization).
// Owned by Engine; GameState holds a non-owning reference for stage and UI access.
class GameSettings
{
public:
    static constexpr const char* kDefaultPath = "user_settings.json";

    const GameRulesConfig_t& GetGameRules() const { return m_gameRules; }
    GameRulesConfig_t& GetGameRules() { return m_gameRules; }
    void SetGameRules(const GameRulesConfig_t& rConfig) { m_gameRules = rConfig; }

    const DebugOptionsConfig_t& GetDebugOptions() const { return m_debugOptions; }
    DebugOptionsConfig_t& GetDebugOptions() { return m_debugOptions; }
    void SetDebugOptions(const DebugOptionsConfig_t& rConfig) { m_debugOptions = rConfig; }

    // Convenience accessors used by UI and turn flow.
    bool IsPauseAtEndOfTurn() const { return m_gameRules.pauseAtEndOfTurn; }
    void SetPauseAtEndOfTurn(bool value) { m_gameRules.pauseAtEndOfTurn = value; }

    const MapGenerationConfig_t& GetMapGeneration() const { return m_mapGeneration; }
    MapGenerationConfig_t& GetMapGeneration() { return m_mapGeneration; }
    void SetMapGeneration(const MapGenerationConfig_t& rConfig) { m_mapGeneration = rConfig; }

    // Missing file leaves defaults (pause off, shroud/fog on, default map generation).
    // Unreadable or corrupt file throws.
    void Load(const std::string& path = kDefaultPath);
    void Save(const std::string& path = kDefaultPath) const;

private:
    GameRulesConfig_t m_gameRules;
    DebugOptionsConfig_t m_debugOptions;
    MapGenerationConfig_t m_mapGeneration;
};

} // namespace ac
