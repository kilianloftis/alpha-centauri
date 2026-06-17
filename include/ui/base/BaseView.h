#pragma once

#include "ui/UIGroup.h"
#include "ui/base/BaseWorkableAreaDisplay.h"
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace ac
{

class BaseManager;
class Graphics;
class PopulationDisplay;
class WorldMap;

class BaseView : public UIGroup
{
public:
    BaseView(
        BaseManager& rBase,
        const WorldMap& rWorldMap,
        Graphics& rGraphics,
        std::function<void()> onClose
    );
    ~BaseView();

    void HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouse(const MouseEvent_t& rEvent) override;
};

} // namespace ac
