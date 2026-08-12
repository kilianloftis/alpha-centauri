#pragma once

#include "input/PlatformEventQueue.h"
#include "lib/Signal.h"

#include <memory>
#include <string>

namespace ac
{

class Graphics;
class Input;
class TurnStageFactory;
class TurnProcessor;
class GameState;
class EventBridge;
class GameDataContext;
class ViewFactory;
class UIManager;
class InteractionPresenter;
class GameSettings;
class Faction;

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
    // Turn pipeline and views. Requires a session to bind to, so it runs last.
    void InitializeUi_();
    void PrintWelcome_() const;
    void GameLoop_();
    void ProcessTurn_();

    // Declared before both backends: the graphics backend writes into it and the input backend
    // reads from it, so it must outlive them.
    PlatformEventQueue m_platformEvents;
    // Before the backends too: the window is opened from these.
    std::unique_ptr<GameSettings> m_pSettings;
    std::unique_ptr<Graphics> m_pGraphics;
    std::unique_ptr<Input> m_pInput;
    // Declared before every live-state member below: Faction, BaseManager, and
    // TileEffectsContext all hold non-owning references into the definition data, so it
    // must outlive them. Members are destroyed in reverse declaration order, so GameState
    // (and every faction/base/unit it owns) is torn down while this is still alive.
    std::unique_ptr<GameDataContext> m_gameDataContext;
    // Resolved once in Initialize_ (map config seed, or a drawn one when that is 0) and used to
    // derive every per-faction seed, so a session's random choices are reproducible.
    unsigned int m_sessionSeed = 0;
    std::unique_ptr<TurnStageFactory> m_turnStageFactory;
    std::unique_ptr<TurnProcessor> m_turnProcessor;
    std::unique_ptr<GameState> m_pGameState;
    std::unique_ptr<EventBridge> m_eventBridge;
    std::unique_ptr<ViewFactory> m_viewFactory;
    std::unique_ptr<UIManager> m_uiManager;
    std::unique_ptr<InteractionPresenter> m_interactionPresenter;
    // Must outlive disconnect: council is owned by GameState above.
    Signal<Faction&, const std::string&>::ScopedConnection m_councilAiVoteConn;
};

} // namespace ac
