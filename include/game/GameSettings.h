#pragma once

#include "game/GameRulesConfig.h"
#include "game/VisibilityConfig.h"
#include "game/map/MapGenerationConfig.h"
#include "graphics/Graphics.h"
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

    bool IsAutoReturnLowFuelAir() const { return m_gameRules.autoReturnLowFuelAir; }
    void SetAutoReturnLowFuelAir(bool value);

    const MapGenerationConfig_t& GetMapGeneration() const { return m_mapGeneration; }
    void SetMapGeneration(const MapGenerationConfig_t& rConfig);

    // Read once, when Engine builds the graphics backend; there is no runtime re-apply.
    const GraphicsConfig_t& GetGraphics() const { return m_graphics; }

    // Missing file leaves defaults (pause off, lethal low-fuel auto-return on, shroud/fog
    // on, default map generation).
    // Unreadable or corrupt file throws. Remembers the path, so Save() round-trips to wherever
    // this object was loaded from.
    void Load(const std::string& path = kDefaultPath);

    // Writes to the remembered path. A settings object that has never been loaded writes to
    // kDefaultPath *relative to the current working directory*, which is why the path is
    // remembered rather than defaulted at every call site: the settings UI saves on every
    // toggle, and a process run from elsewhere would otherwise scribble a fresh file there.
    void Save() const;
    void Save(const std::string& path) const;

    // Point this object at a different file without reading it. For tests and for any future
    // profile switching; without it, exercising the settings UI overwrites the developer's own
    // user_settings.json.
    void SetSavePath(std::string path) { m_path = std::move(path); }

    Signal<> OnGameRulesChanged;
    Signal<> OnVisibilityChanged;
    Signal<> OnMapGenerationChanged;

private:
    GameRulesConfig_t m_gameRules;
    VisibilityConfig_t m_visibility;
    MapGenerationConfig_t m_mapGeneration;
    GraphicsConfig_t m_graphics;
    // Where Save() writes. Set by Load / SetSavePath; kDefaultPath until then.
    std::string m_path = kDefaultPath;
};

} // namespace ac
