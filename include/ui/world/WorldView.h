#pragma once

#include "ui/IGameView.h"
#include "ui/world/CameraInputController.h"
#include "ui/world/UnitOrderInputController.h"
#include "ui/world/WorldDisplay.h"
#include "input/Input.h"
#include <functional>
#include <memory>
#include <utility>

namespace ac
{

class GameState;
class BaseManager;
class Graphics;
class Unit;
class WorldMap;

class WorldView : public IGameView
{
public:
    static constexpr RatioLayout_t k_MapLayout      {0.0f, 0.0f, 1.0f, 0.867f};
    static constexpr RatioLayout_t k_InfoPanelLayout{0.0f, 0.867f, 1.0f, 0.133f};

    using OpenBaseCallback_t = std::function<void(BaseManager&)>;

    WorldView(
        GameState& rGameState,
        const WorldMap& rWorldMap,
        WindowLayout_t layout,
        std::function<void()> onProcessTurn,
        std::function<void()> onRequestExit,
        OpenBaseCallback_t onOpenBase
    );

    void Render(Graphics& rGraphics) override;
    bool HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouse(const MouseEvent_t& rEvent) override;

private:
    void Update_();
    BaseManager* FindBaseAtTile_(int tileX, int tileY) const;
    void SelectUnitAtTile_(int tileX, int tileY);

    GameState& m_rGameState;
    const WindowLayout_t m_mapLayout;
    std::unique_ptr<WorldDisplay> m_pWorldDisplay;
    std::function<void()> m_onProcessTurn;
    std::function<void()> m_onRequestExit;
    OpenBaseCallback_t m_onOpenBase;

    Unit* m_pSelectedUnit = nullptr;

    std::unique_ptr<CameraInputController> m_pCameraInputController;
    std::unique_ptr<UnitOrderInputController> m_pUnitOrderInputController;
};

} // namespace ac
