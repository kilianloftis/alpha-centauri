#pragma once

#include "ui/IGameView.h"
#include "ui/base/BaseDisplaySnapshot.h"
#include "game/buildings/BuildingConfig.h"
#include "lib/Signal.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace ac
{

class BaseManager;
class Faction;
class Graphics;
class Pop;
class Tile;
class MapRenderer;
class SpriteLibrary;
class Unit;
class UnitStackPanel;
struct ScrapPayout_t;

class BaseView : public IGameView
{
public:
    // Editable only for the player's own base. The base's owner is the only faction this view
    // needs: pop types are offered only when it is editable.
    BaseView(
        BaseManager& rBase,
        WindowLayout_t layout,
        MapRenderer& rMapRenderer,
        SpriteLibrary& rSprites
    );
    ~BaseView();

    void Render(Graphics& rGraphics) override;
    bool HandleKey(const KeyEvent_t& rEvent) override;

    // How many times the snapshot was actually rebuilt. The avoided work is the point of the
    // key, and it is not observable from what the panels draw.
    uint64_t GetSnapshotBuildCount() const { return m_snapshotBuildCount; }

private:
    void BuildTopPanel_(MapRenderer& rMapRenderer);
    void BuildLeftPanel_();
    void BuildCenterPanel_();
    void BuildUnitStack_();
    void RefreshUnitStack_();
    void RefreshSnapshot_();
    void HandleTileClick_(const Tile* pTile);
    void HandleBaseClicked_();
    void HandlePopClick_(Pop& rPop);
    void HandleProductionDisplayClicked_();
    void HandleHurryClicked_();
    void HandleHurryConfirmed_(int credits);
    void HandleBuildingClicked_(const BuildingConfig_t& rBuilding);
    void OfferScrapHere_(const BuildingId_t& buildingId);
    void OfferScrapEverywhere_(const BuildingId_t& buildingId);
    void ConfirmScrap_(const std::optional<ScrapPayout_t>& payout,
                       std::function<void()> onConfirmed);
    void ShowNotice_(const std::string& rTitle, const std::string& rMessage);
    void HandleUnitStackClicked_(Unit& rUnit);

    BaseManager& m_rBase;
    // Rebuilt only when one of its key inputs moves; panels hold a reference to it, so it must
    // be declared before m_elements (IGameView) is populated and must never be reallocated.
    BaseDisplaySnapshot_t m_snapshot;
    uint64_t m_snapshotBuildCount = 1;
    // The owner when opened; Render closes the view once the base has changed hands.
    Faction* m_pOwnerAtOpen;
    bool m_bEditable;
    SpriteLibrary& m_rSprites;
    UnitStackPanel* m_pUnitStackPanel = nullptr;
    Unit* m_pSelectedUnit = nullptr;
    // Closes the view when the base is destroyed; goes inert if the base's signal dies first.
    Signal<>::ScopedConnection m_destroyedConnection;
};

} // namespace ac
