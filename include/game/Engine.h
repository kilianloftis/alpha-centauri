#pragma once

#include <memory>
#include <optional>
#include <utility>
#include <string>

#include "game/faction/base/BaseManager.h"

namespace ac
{

class Graphics;
class Input;
class TurnStageFactory;
class TurnProcessor;
class GameState;
class EventBus;
class EventBridge;
class BaseWorkableAreaDisplay;
class WorldDisplay;
class GameDataContext;
class UIManager;

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
    void Render_();
    void HandleMouseInput_();
    void HandleKeyInput_();

    void RenderWorldView_();
    void RenderBaseView_();
    void HandleWorldViewMouse_(int mouseX, int mouseY);
    void HandleBaseViewMouse_(int mouseX, int mouseY);

    BaseManager* FindBaseAtTile_(int tileX, int tileY) const;
    void OpenBaseView_(BaseManager* pBase);
    void ReturnToWorldView_();

    std::unique_ptr<Graphics> m_graphics;
    std::unique_ptr<Input> m_input;
    std::unique_ptr<TurnStageFactory> m_turnStageFactory;
    std::unique_ptr<TurnProcessor> m_turnProcessor;
    std::unique_ptr<GameState> m_gameState;
    std::unique_ptr<EventBus> m_eventBus;
    std::unique_ptr<EventBridge> m_eventBridge;
    std::unique_ptr<WorldDisplay> m_worldDisplay;
    std::unique_ptr<BaseWorkableAreaDisplay> m_workableAreaDisplay;
    std::unique_ptr<GameDataContext> m_gameDataContext;

    enum class ViewMode
    {
        World,
        Base,
    };

    bool m_bShouldExit = false;
    ViewMode m_activeView = ViewMode::World;
    BaseManager* m_pActiveBase = nullptr;

    std::optional<std::pair<int, int>> m_lastClickedTile;
    std::string m_lastClickedTileText;

    static constexpr float kWorldTileSize = 50.f;
    static constexpr float kWorldOriginX = 20.f;
    static constexpr float kWorldOriginY = 60.f;
    static constexpr float kBaseAreaCenterX = 400.f;
    static constexpr float kBaseAreaCenterY = 300.f;
    static constexpr float kBaseTileSize = 50.f;
    std::unique_ptr<UIManager> m_uiManager;
};

} // namespace ac
