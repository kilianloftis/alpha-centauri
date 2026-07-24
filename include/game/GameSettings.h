#pragma once

#include "game/GameRulesConfig.h"
#include "game/VisibilityConfig.h"
#include "game/map/MapGenerationConfig.h"
#include "lib/Signal.h"
#include <string>

namespace ac
{

// Cross-save player preferences (not part of GameState / save-game serialization).
// Owned by Engine; GameState holds a non-owning reference for stage and UI access.
// Domain getters are const-only; mutate via Set* (emits the matching domain signal when
// the value actually changes).
class GameSettings
{
public:
    static constexpr const char* kDefaultPath = "user_settings.json";

    const GameRulesConfig_t& GetGameRules() const { return m_gameRules; }
    void SetGameRules(const GameRulesConfig_t& rConfig);

    const VisibilityConfig_t& GetVisibility() const { return m_visibility; }
    void SetVisibility(const VisibilityConfig_t& rConfig);

    // Convenience accessors used by UI and turn flow.
    bool IsPauseAtEndOfTurn() const { return m_gameRules.pauseAtEndOfTurn; }
    void SetPauseAtEndOfTurn(bool value);

    const MapGenerationConfig_t& GetMapGeneration() const { return m_mapGeneration; }
    void SetMapGeneration(const MapGenerationConfig_t& rConfig);

    // Missing file leaves defaults (pause off, shroud/fog on, default map generation).
    // Unreadable or corrupt file throws.
    void Load(const std::string& path = kDefaultPath);
    void Save(const std::string& path = kDefaultPath) const;

    Signal<> OnGameRulesChanged;
    Signal<> OnVisibilityChanged;
    Signal<> OnMapGenerationChanged;

private:
    GameRulesConfig_t m_gameRules;
    VisibilityConfig_t m_visibility;
    MapGenerationConfig_t m_mapGeneration;
};

} // namespace ac
