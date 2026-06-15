#pragma once

#include "ui/IGameView.h"
#include "ui/base/IBasePanel.h"
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace ac
{

class BaseDisplay;
class BaseManager;
class BaseWorkableAreaDisplay;
class EventBus;
class Graphics;
class PopulationDisplay;
class UIManager;

class BaseView : public IGameView
{
public:
    static constexpr float kScreenWidth = 800.f;
    static constexpr float kScreenHeight = 600.f;

    BaseView(
        BaseManager& rBase,
        BaseWorkableAreaDisplay& rWorkableAreaDisplay,
        EventBus& rBus,
        Graphics& rGraphics,
        UIManager& rUIManager
    );
    ~BaseView();

    void OnPopped() override;
    void Render(Graphics& rGraphics) override;
    void Update(float deltaTime) override;
    void HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouse(const MouseEvent_t& rEvent) override;

private:
    BaseManager& m_rBase;
    BaseWorkableAreaDisplay& m_rWorkableAreaDisplay;
    UIManager& m_rUIManager;
    std::unique_ptr<BaseDisplay> m_pBaseDisplay;
    std::unique_ptr<PopulationDisplay> m_pPopDisplay;
    std::vector<IBasePanel*> m_panels;
    std::optional<std::pair<int, int>> m_lastClickedTile;
};

} // namespace ac
