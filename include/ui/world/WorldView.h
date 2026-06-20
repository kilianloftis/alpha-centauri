#pragma once

#include "ui/UIGroup.h"
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
class WorldMap;

class WorldView : public UIGroup
{
public:
    static constexpr RatioLayout_t k_MapLayout      {0.0f, 0.0f, 1.0f, 0.867f};
    static constexpr RatioLayout_t k_InfoPanelLayout{0.0f, 0.867f, 1.0f, 0.133f};

    using PushViewCallback_t = std::function<void(std::unique_ptr<UIGroup>)>;

    WorldView(
        GameState& rGameState,
        const WorldMap& rWorldMap,
        WindowLayout_t layout,
        std::function<void()> onProcessTurn,
        std::function<void()> onRequestExit,
        PushViewCallback_t onPushView,
        std::function<std::unique_ptr<UIGroup>(BaseManager*)> onOpenBase
    );

    void Render(Graphics& rGraphics) override;
    void HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouse(const MouseEvent_t& rEvent) override;

private:
    void Update_();
    BaseManager* FindBaseAtTile_(int tileX, int tileY) const;

    GameState& m_rGameState;
    std::unique_ptr<WorldDisplay> m_pWorldDisplay;
    std::function<void()> m_onProcessTurn;
    std::function<void()> m_onRequestExit;
    PushViewCallback_t m_onPushView;
    std::function<std::unique_ptr<UIGroup>(BaseManager*)> m_onOpenBase;
};

} // namespace ac
