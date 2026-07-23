#pragma once

#include "ui/IGameView.h"
#include <memory>
#include <vector>

namespace ac
{

class BaseManager;
class Faction;
class Graphics;
class Pop;
class Tile;
class Unit;
class UnitStackPanel;
struct PopTypeConfig_t;

class BaseView : public IGameView
{
public:
    BaseView(
        BaseManager& rBase,
        const Faction& rFaction,
        WindowLayout_t layout,
        bool bEditable
    );
    ~BaseView();

    void Render(Graphics& rGraphics) override;
    bool HandleKey(const KeyEvent_t& rEvent) override;

private:
    void RefreshUnitStack_();
    void HandleTileClick_(const Tile* pTile);
    void HandleBaseClicked_();
    void HandlePopClick(Pop& rPop);
    void HandlePopTypeSelected(Pop& rPop, const PopTypeConfig_t& rConfig);
    void HandleProductionDisplayClicked_();
    void HandleUnitStackClicked_(Unit& rUnit);

    BaseManager& m_rBase;
    const Faction& m_rFaction;
    bool m_bEditable;
    UnitStackPanel* m_pUnitStackPanel = nullptr;
    Unit* m_pSelectedUnit = nullptr;
};

} // namespace ac
