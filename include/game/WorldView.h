#pragma once

#include "ui/IGameView.h"
#include "ui/WorldMapElement.h"
#include "ui/InfoPanelElement.h"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace ac
{

class GameState;
class WorldDisplay;
class UIManager;
class BaseManager;

class WorldView : public IGameView
{
public:
    static constexpr float kWindowWidth = 800.f;
    static constexpr float kWindowHeight = 600.f;
    static constexpr float kInfoPanelHeight = 80.f;

    WorldView(
        GameState& rGameState,
        WorldDisplay& rWorldDisplay,
        UIManager& rUIManager,
        std::function<void()> onProcessTurn,
        std::function<std::unique_ptr<IGameView>(BaseManager*)> onOpenBase
    );

    void Render(Graphics& rGraphics) override;
    void Update(float deltaTime) override;
    bool HandleKey(const KeyEvent_t& rEvent) override;
    bool HandleMouse(const MouseEvent_t& rEvent) override;
    std::vector<UIElement*> GetElements() override;

private:
    BaseManager* FindBaseAtTile_(int tileX, int tileY) const;

    GameState& m_rGameState;
    WorldDisplay& m_rWorldDisplay;
    UIManager& m_rUIManager;
    std::function<void()> m_onProcessTurn;
    std::function<std::unique_ptr<IGameView>(BaseManager*)> m_onOpenBase;
    std::unique_ptr<WorldMapElement> m_pWorldMap;
    std::unique_ptr<InfoPanelElement> m_pInfoPanel;
    std::optional<std::pair<int, int>> m_lastClickedTile;
    std::string m_lastClickedTileText;
};

} // namespace ac
