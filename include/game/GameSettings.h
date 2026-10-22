#pragma once

#include "game/GameRulesConfig.h"
#include "game/MapDisplayConfig.h"
#include "game/PauseOnEventsConfig.h"
#include "game/visibility/VisibilityConfig.h"
#include "game/worldgen/MapGenerationConfig.h"
#include "graphics/Graphics.h"
#include "lib/Revision.h"
#include "lib/Signal.h"
#include <string>

namespace ac
{

// Player preferences kept across saves, owned by Engine. Set* emits the domain's signal when the
// value changes.
class GameSettings
{
public:
    static constexpr const char* k_DefaultPath = "user_settings.json";

    const GameRulesConfig_t& GetGameRules() const { return m_gameRules; }
    void SetGameRules(const GameRulesConfig_t& rConfig);
    // Bumped when game rules change, so a difficulty switch refreshes every faction's effects.
    const Revision& GetGameRulesRevision() const { return m_gameRulesRevision; }

    const VisibilityConfig_t& GetVisibility() const { return m_visibility; }
    void SetVisibility(const VisibilityConfig_t& rConfig);

    const PauseOnEventsConfig_t& GetPauseOnEvents() const { return m_pauseOnEvents; }
    void SetPauseOnEvents(const PauseOnEventsConfig_t& rConfig);

    bool IsPauseAtEndOfTurn() const { return m_gameRules.pauseAtEndOfTurn; }
    void SetPauseAtEndOfTurn(bool value);

    bool IsAutoReturnLowFuelAir() const { return m_gameRules.autoReturnLowFuelAir; }
    void SetAutoReturnLowFuelAir(bool value);

    const MapGenerationConfig_t& GetMapGeneration() const { return m_mapGeneration; }
    void SetMapGeneration(const MapGenerationConfig_t& rConfig);

    const MapDisplayConfig_t& GetMapDisplay() const { return m_mapDisplay; }
    void SetMapDisplay(const MapDisplayConfig_t& rConfig);

    // Read once, when Engine builds the graphics backend; there is no runtime re-apply.
    const GraphicsConfig_t& GetGraphics() const { return m_graphics; }

    // A missing file leaves the defaults; an unreadable one throws. Remembers the path for Save.
    void Load(const std::string& path);

    // Writes to the remembered path, or k_DefaultPath under the working directory if never loaded.
    void Save() const;
    void Save(const std::string& path) const;

    // Points Save at another file, so tests do not overwrite the developer's settings.
    void SetSavePath(std::string path) { m_path = std::move(path); }

    Signal<> OnGameRulesChanged;
    Signal<> OnVisibilityChanged;
    Signal<> OnPauseOnEventsChanged;
    Signal<> OnMapGenerationChanged;
    Signal<> OnMapDisplayChanged;

private:
    GameRulesConfig_t m_gameRules;
    Revision m_gameRulesRevision;
    VisibilityConfig_t m_visibility;
    PauseOnEventsConfig_t m_pauseOnEvents;
    MapGenerationConfig_t m_mapGeneration;
    MapDisplayConfig_t m_mapDisplay;
    GraphicsConfig_t m_graphics;
    // Where Save() writes. Set by Load / SetSavePath; k_DefaultPath until then.
    std::string m_path = k_DefaultPath;
};

} // namespace ac
