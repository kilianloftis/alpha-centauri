#pragma once

#include "ui/IGameView.h"
#include "lib/Signal.h"
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
    // The base's owner at the moment this view was opened. Render() closes the view (rather
    // than keep rendering a base that changed hands out from under it) when
    // &m_rBase.GetFaction() no longer matches — see docs/architecture/high-level.md, "Object
    // lifetime". Distinct from m_rFaction, which is always the *viewing* player faction (used
    // for editability / available pop types), not necessarily the base's owner.
    Faction* m_pOwnerAtOpen;
    bool m_bEditable;
    UnitStackPanel* m_pUnitStackPanel = nullptr;
    Unit* m_pSelectedUnit = nullptr;
    // Pops this view when the base is destroyed (razed on capture) while it is still open —
    // the object dies but this reference must not dangle. Safe to outlive the base: the
    // connection goes inert once BaseManager's signal is gone (see Signal::ScopedConnection).
    Signal<>::ScopedConnection m_destroyedConnection;
};

} // namespace ac
