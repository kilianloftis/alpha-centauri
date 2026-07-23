#pragma once

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

    bool IsPauseAtEndOfTurn() const { return m_bPauseAtEndOfTurn; }
    void SetPauseAtEndOfTurn(bool value) { m_bPauseAtEndOfTurn = value; }

    const MapGenerationConfig_t& GetMapGeneration() const { return m_mapGeneration; }
    MapGenerationConfig_t& GetMapGeneration() { return m_mapGeneration; }
    void SetMapGeneration(const MapGenerationConfig_t& rConfig) { m_mapGeneration = rConfig; }

    // Missing file leaves defaults (pause off, default map generation).
    // Unreadable or corrupt file throws.
    void Load(const std::string& path = kDefaultPath);
    void Save(const std::string& path = kDefaultPath) const;

private:
    bool m_bPauseAtEndOfTurn = false;
    MapGenerationConfig_t m_mapGeneration;
};

} // namespace ac
