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
class Faction;
class Graphics;
class PopulationDisplay;
class WorldMap;

class BaseView : public UIGroup
{
public:
    BaseView(
        BaseManager& rBase,
        const WorldMap& rWorldMap,
        const Faction& rFaction,
        WindowLayout_t layout
    );
    ~BaseView();

    void HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouse(const MouseEvent_t& rEvent) override;

private:
    void HandlePopClick(Pop& rPop);
    void HandlePopTypeSelected(Pop& rPop, const PopTypeConfig& rConfig);

    BaseManager& m_rBase;
    const Faction& m_rFaction;

    static constexpr WindowLayout_t k_WorkableAreaLayout{0.0f, 0.0f, 1.0f, 0.7f};
    static constexpr WindowLayout_t k_BottomPanelLayout{0.0f, 0.7f, 1.0f, 0.3f};
    static constexpr WindowLayout_t k_PopupLayout{0.3f, 0.3f, 0.4f, 0.4f};  // Centered 40% of screen
};

} // namespace ac
