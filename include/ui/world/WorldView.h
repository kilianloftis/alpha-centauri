#pragma once

#include "ui/IGameView.h"
#include "ui/world/WorldDisplay.h"
#include "input/Input.h"
#include <chrono>
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
    bool HandleCameraKey_(const KeyEvent_t& rEvent);
    BaseManager* FindBaseAtTile_(int tileX, int tileY) const;
    void SelectUnitAtTile_(int tileX, int tileY);

    GameState& m_rGameState;
    std::unique_ptr<WorldDisplay> m_pWorldDisplay;
    std::function<void()> m_onProcessTurn;
    std::function<void()> m_onRequestExit;
    OpenBaseCallback_t m_onOpenBase;

    Unit* m_pSelectedUnit = nullptr;

    bool m_bRightButtonHeld = false;
    std::chrono::steady_clock::time_point m_rightButtonPressTime;
};

} // namespace ac
