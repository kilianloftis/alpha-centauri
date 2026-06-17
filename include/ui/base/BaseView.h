#pragma once

#include "ui/IGameView.h"
#include "ui/base/IBasePanel.h"
#include "ui/base/BaseWorkableAreaDisplay.h"
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace ac
{

class BaseDisplay;
class BaseManager;
class EventBus;
class Graphics;
class PopulationDisplay;
class UIManager;
class WorldMap;

class BaseView : public IGameView
{
public:
    BaseView(
        BaseManager& rBase,
        const WorldMap& rWorldMap,
        Graphics& rGraphics,
        std::function<void()> onClose
    );
    ~BaseView();

    void Render(Graphics& rGraphics) override;
    void Update() override;
    void HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouse(const MouseEvent_t& rEvent) override;

private:
    BaseManager& m_rBase;
    std::unique_ptr<BaseWorkableAreaDisplay> m_pWorkableAreaDisplay;
    std::unique_ptr<BaseDisplay> m_pBaseDisplay;
    std::unique_ptr<PopulationDisplay> m_pPopDisplay;
};

} // namespace ac
