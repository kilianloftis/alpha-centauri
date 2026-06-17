#pragma once

#include "ui/IGameView.h"
#include "ui/world/WorldDisplay.h"
#include "ui/world/WorldMapElement.h"
#include "ui/world/InfoPanelElement.h"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace ac
{

class GameState;
class UIManager;
class BaseManager;
class Graphics;
class WorldMap;

class WorldView : public IGameView
{
public:
    static constexpr float kWindowWidth = 800.f;
    static constexpr float kWindowHeight = 600.f;
    static constexpr float kInfoPanelHeight = 80.f;

    WorldView(
        GameState& rGameState,
        Graphics& rGraphics,
        const WorldMap& rWorldMap,
        UIManager& rUIManager,
        std::function<void()> onProcessTurn,
        std::function<std::unique_ptr<IGameView>(BaseManager*)> onOpenBase
    );

    void Render(Graphics& rGraphics) override;
    void Update(float deltaTime) override;
    void HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouse(const MouseEvent_t& rEvent) override;

private:
    BaseManager* FindBaseAtTile_(int tileX, int tileY) const;

    GameState& m_rGameState;
    std::unique_ptr<WorldDisplay> m_pWorldDisplay;
    UIManager& m_rUIManager;
    std::function<void()> m_onProcessTurn;
    std::function<std::unique_ptr<IGameView>(BaseManager*)> m_onOpenBase;
    std::unique_ptr<WorldMapElement> m_pWorldMap;
    std::unique_ptr<InfoPanelElement> m_pInfoPanel;
    std::optional<std::pair<int, int>> m_lastClickedTile;
    std::string m_lastClickedTileText;
};

} // namespace ac
