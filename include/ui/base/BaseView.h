#pragma once

#include "ui/IGameView.h"
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
struct PopTypeConfig_t;
class Tile;

class BaseView : public IGameView
{
public:
    BaseView(
        BaseManager& rBase,
        const Faction& rFaction,
        WindowLayout_t layout
    );
    ~BaseView();

    bool HandleKey(const KeyEvent_t& rEvent) override;

private:
    void HandleTileClick_(const Tile* pTile);
    void HandleBaseClicked_();
    void HandlePopClick(Pop& rPop);
    void HandlePopTypeSelected(Pop& rPop, const PopTypeConfig_t& rConfig);
    void HandleProductionDisplayClicked_();

    BaseManager& m_rBase;
    const Faction& m_rFaction;

};

} // namespace ac
