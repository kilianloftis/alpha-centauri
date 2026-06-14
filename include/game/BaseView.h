#pragma once

#include "ui/IGameView.h"
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace ac
{

class BaseManager;
class BaseWorkableAreaDisplay;
class UIManager;

class BaseView : public IGameView
{
public:
    static constexpr float kBaseAreaCenterX = 400.f;
    static constexpr float kBaseAreaCenterY = 300.f;
    static constexpr float kBaseTileSize = 50.f;

    BaseView(
        BaseManager& rBase,
        BaseWorkableAreaDisplay& rWorkableAreaDisplay,
        UIManager& rUIManager
    );

    void OnPopped() override;
    void Render(Graphics& rGraphics) override;
    void Update(float deltaTime) override;
    bool HandleKey(const KeyEvent_t& rEvent) override;
    bool HandleMouse(const MouseEvent_t& rEvent) override;
    std::vector<UIElement*> GetElements() override;

private:
    BaseManager& m_rBase;
    BaseWorkableAreaDisplay& m_rWorkableAreaDisplay;
    UIManager& m_rUIManager;
    std::optional<std::pair<int, int>> m_lastClickedTile;
    std::string m_lastClickedTileText;
};

} // namespace ac
