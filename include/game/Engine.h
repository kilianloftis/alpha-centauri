#pragma once

#include <memory>

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
class GameSettings;

class Engine
{
public:
    Engine();
    ~Engine();
    void Run();

private:
    void Initialize_();
    void PrintWelcome_() const;
    void GameLoop_();
    void ProcessTurn_();

    std::unique_ptr<Graphics> m_pGraphics;
    std::unique_ptr<Input> m_pInput;
    std::unique_ptr<GameSettings> m_pSettings;
    std::unique_ptr<TurnStageFactory> m_turnStageFactory;
    std::unique_ptr<TurnProcessor> m_turnProcessor;
    std::unique_ptr<GameState> m_pGameState;
    std::unique_ptr<EventBridge> m_eventBridge;
    std::unique_ptr<GameDataContext> m_gameDataContext;
    std::unique_ptr<ViewFactory> m_viewFactory;
    std::unique_ptr<UIManager> m_uiManager;
};

} // namespace ac
