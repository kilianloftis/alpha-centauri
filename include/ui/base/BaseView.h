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
class Pop;
class PopulationDisplay;
struct PopTypeConfig;
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

private:
    void HandleTileClick_(int tileX, int tileY);
    void HandlePopClick(Pop& rPop);
    void HandlePopTypeSelected(Pop& rPop, const PopTypeConfig& rConfig);

    BaseManager& m_rBase;
    const Faction& m_rFaction;

    static constexpr RatioLayout_t k_WorkableAreaLayout{0.0f, 0.0f, 1.0f, 0.7f};
    static constexpr RatioLayout_t k_BottomPanelLayout{0.0f, 0.7f, 1.0f, 0.3f};
    static constexpr RatioLayout_t k_PopupLayout{0.3f, 0.3f, 0.4f, 0.4f};
};

} // namespace ac
