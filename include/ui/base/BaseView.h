#pragma once

#include "ui/IGameView.h"
#include "ui/base/BaseDisplaySnapshot.h"
#include "lib/Signal.h"
#include <cstdint>
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
    // The base's owner is the only faction this view needs: pop types are offered only when
    // the view is editable, which ViewFactory grants only for the player's own base.
    BaseView(
        BaseManager& rBase,
        WindowLayout_t layout,
        bool bEditable
    );
    ~BaseView();

    void Render(Graphics& rGraphics) override;
    bool HandleKey(const KeyEvent_t& rEvent) override;

    // How many times the snapshot was actually rebuilt. The avoided work is the point of the
    // key, and it is not observable from what the panels draw.
    uint64_t GetSnapshotBuildCount() const { return m_snapshotBuildCount; }

private:
    void RefreshUnitStack_();
    void RefreshSnapshot_();
    void HandleTileClick_(const Tile* pTile);
    void HandleBaseClicked_();
    void HandlePopClick_(Pop& rPop);
    void HandlePopTypeSelected_(Pop& rPop, const PopTypeConfig_t& rConfig);
    void HandleProductionDisplayClicked_();
    void HandleUnitStackClicked_(Unit& rUnit);

    BaseManager& m_rBase;
    // Rebuilt only when one of its key inputs moves; panels hold a reference to it, so it must
    // be declared before m_elements (IGameView) is populated and must never be reallocated.
    BaseDisplaySnapshot_t m_snapshot;
    uint64_t m_snapshotBuildCount = 1;
    // The base's owner at the moment this view was opened. Render() closes the view (rather
    // than keep rendering a base that changed hands out from under it) when
    // &m_rBase.GetFaction() no longer matches — see docs/architecture/high-level.md, "Object
    // lifetime".
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
