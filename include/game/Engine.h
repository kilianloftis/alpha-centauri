#pragma once

#include "input/PlatformEventQueue.h"
#include "lib/Signal.h"
#include "ui/UIElement.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ac
{

class Graphics;
class Input;
class TurnStageFactory;
class TurnProcessor;
class GameState;
class GameStateScope;
class EventBridge;
struct GameDataContext;
class HotkeyConfig;
class StandingCaptions;
class TalkScript;
class TextPack;
class ViewFactory;
class SpriteLibrary;
class MapRenderer;
class UIManager;
class InteractionPresenter;
class GameSettings;
class Faction;
class BaseManager;
class Tile;
class UnitDesign;
class WorldDisplay;
struct CombatResult_t;
struct FactionConfig_t;

class Engine
{
public:
    Engine();
    ~Engine();
    void Run();

private:
    void Initialize_();
    // Process-wide setup that outlives any session: user settings, UI style, and the parsed
    // game data. Runs once; a future "new game" from the menu must not re-run it.
    void InitializeApp_();
    // Everything a session owns: the resolved seed, the generated world, GameState, factions
    // and their starting assets, and the council. Requires InitializeApp_.
    void StartNewGame_();
    void PickSessionSeed_();
    void GenerateWorld_();
    // Returns the bases placed so far, which later placements keep their distance from.
    std::vector<const BaseManager*> AddFactions_();
    void AddFaction_(const FactionConfig_t& rFactionConfig, size_t positionIndex,
                     const std::vector<std::pair<int, int>>& rPreferredStartPositions,
                     std::vector<const BaseManager*>& rPlacedBases);
    void AddTestUnits_(Faction& rFaction, const Tile& rStart, BaseManager& rBase);
    const UnitDesign& AddTestDesign_(Faction& rFaction, const std::string& rChassisId,
                                     const std::string& rWeaponId, const std::string& rLabel);
    // Places a unit at lattice offset (p, q) from rStart.
    void PlaceTestUnit_(Faction& rFaction, const UnitDesign& rDesign, const Tile& rStart,
                        int p, int q, BaseManager& rBase);
    void SetUpStartingDiplomacy_();
    // Turn pipeline and views. Requires a session to bind to, so it runs last.
    void InitializeUi_();
    void CreateTurnProcessor_();
    void PushCombatView_(CombatResult_t result,
                         const Tile& rAttackerTile,
                         const Tile& rDefenderTile,
                         std::string attackerName,
                         std::string defenderName,
                         WorldDisplay& rWorldDisplay,
                         WindowLayout_t mapLayout,
                         std::function<void()> onFinished);
    void PushCommlinksView_();
    void BindViewShortcuts_(const WindowLayout_t& rFullscreen);
    void PrintWelcome_() const;
    void GameLoop_();
    void ProcessTurn_();

    // Declared before both backends: the graphics backend writes into it and the input backend
    // reads from it, so it must outlive them.
    PlatformEventQueue m_platformEvents;
    // Before the backends too: the window is opened from these.
    std::unique_ptr<GameSettings> m_pSettings;
    std::unique_ptr<Graphics> m_pGraphics;
    // Over m_pGraphics, so it is declared after it and torn down before it.
    std::unique_ptr<SpriteLibrary> m_pSprites;
    std::unique_ptr<Input> m_pInput;
    // Before every live-state member: factions, bases and tile effects hold references into it.
    std::unique_ptr<const GameDataContext> m_gameDataContext;
    // Resolved once by StartNewGame_; every per-faction seed derives from it.
    unsigned int m_sessionSeed = 0;
    std::unique_ptr<TurnStageFactory> m_turnStageFactory;
    std::unique_ptr<TurnProcessor> m_turnProcessor;
    std::unique_ptr<GameState> m_pGameState;
    std::unique_ptr<GameStateScope> m_pGameStateScope;
    std::unique_ptr<EventBridge> m_eventBridge;
    // Before the views: WorldView and ViewFactory borrow this for the session.
    std::unique_ptr<HotkeyConfig> m_pHotkeys;
    std::unique_ptr<StandingCaptions> m_pStandingCaptions;
    std::unique_ptr<TextPack> m_pTextPack;
    std::unique_ptr<TalkScript> m_pTalkScript;
    // Before the views for the same reason; built once the UI style is loaded.
    std::unique_ptr<MapRenderer> m_pMapRenderer;
    std::unique_ptr<ViewFactory> m_viewFactory;
    std::unique_ptr<UIManager> m_uiManager;
    std::unique_ptr<InteractionPresenter> m_interactionPresenter;
    // Must outlive disconnect: council is owned by GameState above.
    Signal<Faction&, const std::string&>::ScopedConnection m_councilAiVoteConn;
};

} // namespace ac
