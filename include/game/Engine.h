#pragma once

#include <memory>

namespace ac
{

class Graphics;
class Input;
class TurnStageFactory;
class TurnProcessor;
class GameState;
class EventBus;
class EventBridge;

class Engine
{
public:
    Engine();
    ~Engine();
    void Run();

private:
    void Initialize_();
    void CheckInitialized_() const;
    void PrintWelcome_() const;
    void GameLoop_();
    void ProcessTurn_();

    std::unique_ptr<Graphics> m_graphics;
    std::unique_ptr<Input> m_input;
    std::unique_ptr<TurnStageFactory> m_turnStageFactory;
    std::unique_ptr<TurnProcessor> m_turnProcessor;
    std::unique_ptr<GameState> m_gameState;
    std::unique_ptr<EventBus> m_eventBus;
    std::unique_ptr<EventBridge> m_eventBridge;
};

} // namespace ac
