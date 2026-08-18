#include "ui/base/BaseView.h"
#include "ui/base/BaseNameDisplay.h"
#include "ui/base/BaseWorkableAreaDisplay.h"
#include "ui/base/BuildQueueDisplay.h"
#include "ui/base/BuildingsDisplay.h"
#include "ui/base/GrowthDisplay.h"
#include "ui/base/HurryProductionPopup.h"
#include "ui/base/ProductionDisplay.h"
#include "ui/base/PopulationDisplay.h"
#include "ui/base/SupportDisplay.h"
#include "ui/ConfirmPopup.h"
#include "ui/ListSelectorPopup.h"
#include "ui/NoticePopup.h"
#include "ui/ScrapRefundText.h"
#include "ui/world/UnitStackPanel.h"
#include "game/population/pop-types/Pop.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/production/HurryProductionCalculator.h"
#include "game/faction/base/production/ScrapConfig.h"
#include "game/faction/base/production/ScrapPayout.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/EconomyManager.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/Faction.h"
#include "ui/style/UiStyle.h"
#include "graphics/Graphics.h"
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

BaseView::BaseView(
    BaseManager& rBase,
    WindowLayout_t layout,
    bool bEditable
)
    : IGameView(layout)
    , m_rBase(rBase)
    , m_snapshot(BuildBaseDisplaySnapshot(rBase))
    , m_pOwnerAtOpen(&rBase.GetFaction())
    , m_bEditable(bEditable)
    , m_destroyedConnection(rBase.OnDestroyed.ConnectScoped([this]() { m_bShouldClose = true; }))
{
    const auto& bv = Style().baseView;
    const WindowLayout_t topPanel = ResolveLayout(m_layout, Style().layouts.topPanel);
    const WindowLayout_t leftPanel = ResolveLayout(m_layout, Style().layouts.leftPanel);
    const WindowLayout_t centerPanel = ResolveLayout(m_layout, Style().layouts.centerPanel);

    BaseWorkableAreaDisplay::TileClickCallback_t onTileClick;
    BaseWorkableAreaDisplay::BaseClickCallback_t onBaseClick;
    std::function<void()> onProductionClick;
    std::function<void()> onHurryClick;
    PopClickCallback_t onPopClick;
    BuildingClickCallback_t onBuildingClick;
    if (m_bEditable)
    {
        onTileClick = [this](const Tile* pTile) { HandleTileClick_(pTile); };
        onBaseClick = [this]() { HandleBaseClicked_(); };
        onProductionClick = [this]() { HandleProductionDisplayClicked_(); };
        onHurryClick = [this]() { HandleHurryClicked_(); };
        onPopClick = [this](Pop& rPop) { HandlePopClick_(rPop); };
        onBuildingClick = [this](const BuildingConfig_t& rBuilding)
        { HandleBuildingClicked_(rBuilding); };
    }

    // TopPanel: Growth | Workable (60%) | Buildings
    m_elements.push_back(std::make_unique<GrowthDisplay>(
        m_rBase,
        m_snapshot,
        ResolveLayout(topPanel, bv.growthLayout)
    ));
    m_elements.push_back(std::make_unique<BaseWorkableAreaDisplay>(
        m_rBase,
        m_snapshot,
        ResolveLayout(topPanel, bv.workableLayout),
        std::move(onTileClick),
        std::move(onBaseClick)
    ));
    m_elements.push_back(std::make_unique<BuildingsDisplay>(
        m_rBase,
        ResolveLayout(topPanel, bv.buildingsLayout),
        std::move(onBuildingClick)
    ));

    // LeftPanel: Production (2/3) | Build Queue (1/3)
    m_elements.push_back(std::make_unique<ProductionDisplay>(
        m_rBase,
        m_snapshot,
        ResolveLayout(leftPanel, bv.productionLayout),
        std::move(onProductionClick)
    ));
    m_elements.push_back(std::make_unique<BuildQueueDisplay>(
        m_rBase,
        ResolveLayout(leftPanel, bv.buildQueueLayout),
        std::move(onHurryClick)
    ));

    // CenterPanel: Base Name (1/3) | Population (2/3)
    m_elements.push_back(std::make_unique<BaseNameDisplay>(
        m_rBase,
        ResolveLayout(centerPanel, bv.baseNameLayout)
    ));
    m_elements.push_back(std::make_unique<PopulationDisplay>(
        m_rBase.GetPopulation(),
        ResolveLayout(centerPanel, bv.populationLayout),
        std::move(onPopClick)
    ));

    // RightPanel: Support
    m_elements.push_back(std::make_unique<SupportDisplay>(
        m_rBase,
        ResolveLayout(m_layout, Style().layouts.rightPanel)
    ));

    // BottomPanel: Unit stack (same component / slot as WorldView)
    auto pUnitStack = std::make_unique<UnitStackPanel>(
        ResolveLayout(m_layout, Style().layouts.bottomPanel),
        [this](Unit& rUnit) { HandleUnitStackClicked_(rUnit); });
    m_pUnitStackPanel = pUnitStack.get();
    m_elements.push_back(std::move(pUnitStack));
}

BaseView::~BaseView()
{
    // Panels hold const BaseDisplaySnapshot_t& into m_snapshot, which is a member of this class
    // and therefore destroyed before IGameView's m_elements. Drop the panels first.
    m_elements.clear();
}

void BaseView::Render(Graphics& rGraphics)
{
    // Base survives identity-preserving transfer (RebindFaction), so &GetFaction() is always
    // safe to read here — unlike destroy, which is instead caught by m_destroyedConnection
    // (OnDestroyed fires while the object is still valid, before UIManager ever renders a
    // popped view again). Pop rather than render a base that changed hands out from under it.
    if (&m_rBase.GetFaction() != m_pOwnerAtOpen)
    {
        m_bShouldClose = true;
        return;
    }

    RefreshSnapshot_();
    RefreshUnitStack_();
    IGameView::Render(rGraphics);
}

void BaseView::RefreshSnapshot_()
{
    // Reading the key is a handful of counter loads; rebuilding is two worked-resource passes
    // and a yield resolution per workable tile.
    if (ReadBaseDisplayKey(m_rBase) == m_snapshot.key)
    {
        return;
    }
    m_snapshot = BuildBaseDisplaySnapshot(m_rBase);
    ++m_snapshotBuildCount;
}

void BaseView::RefreshUnitStack_()
{
    const WorldMap& rMap = m_rBase.GetTileEffects().GetWorldMap();
    const Tile& rTile = m_rBase.GetTile();

    std::vector<Unit*> stackUnits;
    for (Unit* pUnit : rMap.GetUnitsOnTile(rTile))
    {
        if (pUnit)
        {
            stackUnits.push_back(pUnit);
        }
    }

    if (m_pSelectedUnit)
    {
        bool bStillPresent = false;
        for (Unit* pUnit : stackUnits)
        {
            if (pUnit == m_pSelectedUnit)
            {
                bStillPresent = true;
                break;
            }
        }
        if (!bStillPresent)
        {
            m_pSelectedUnit = nullptr;
        }
    }

    m_pUnitStackPanel->SetUnits(std::move(stackUnits), m_pSelectedUnit);
}

void BaseView::HandleUnitStackClicked_(Unit& rUnit)
{
    m_pSelectedUnit = &rUnit;
}

bool BaseView::HandleKey(const KeyEvent_t& rEvent)
{
    if (IGameView::HandleKey(rEvent))
    {
        return true;
    }
    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
        return true;
    }
    return false;
}

void BaseView::HandleBaseClicked_()
{
    if (!m_bEditable)
    {
        return;
    }

    auto& rAssignments = m_rBase.GetWorkerAssignments();
    rAssignments.ResetAllAssignments();
}

void BaseView::HandleTileClick_(const Tile* pTile)
{
    if (!m_bEditable)
    {
        return;
    }

    auto& rAssignments = m_rBase.GetWorkerAssignments();

    // Toggle against what *this* base is doing. UserUnassignTile only scans this base's pops,
    // so routing a click here on a tile a neighbour works silently did nothing; the assign
    // path handles the "taken by someone else" case by failing to find a free tile.
    if (rAssignments.IsTileWorkedByThisBase(pTile))
    {
        rAssignments.UserUnassignTile(pTile);
    }
    else
    {
        m_rBase.UserAssignBestAvailableWorker(pTile);
    }
}

void BaseView::HandlePopClick_(Pop& rPop)
{
    if (!m_bEditable)
    {
        return;
    }

    std::vector<PopupChoice_t> choices;
    for (const PopTypeConfig_t* pConfig : m_rBase.GetFaction().GetAvailablePopTypes())
    {
        choices.push_back(
            {pConfig->name, [this, &rPop, pConfig] { HandlePopTypeSelected_(rPop, *pConfig); }});
    }

    DismissOpenModals_();
    m_elements.push_back(std::make_unique<ListSelectorPopup>(
        "Select Pop Type", "No pop types available", std::move(choices),
        ResolveLayout(m_layout, Style().layouts.popupSmall), Style().listSelectorPopup));
}

void BaseView::HandlePopTypeSelected_(Pop& rPop, const PopTypeConfig_t& rConfig)
{
    if (!m_bEditable)
    {
        return;
    }

    m_rBase.ConvertPop(rPop, rConfig.id);
}

void BaseView::HandleProductionDisplayClicked_()
{
    if (!m_bEditable)
    {
        return;
    }

    std::vector<PopupChoice_t> choices;
    for (const IConstructable* pItem : m_rBase.GetConstructable())
    {
        choices.push_back({pItem->GetName(),
                           [this, pItem]
                           {
                               m_rBase.GetProduction().SetProduction(pItem,
                                                                     m_rBase.GetBaseEffects());
                           }});
    }

    DismissOpenModals_();
    m_elements.push_back(std::make_unique<ListSelectorPopup>(
        "Select Production", "Nothing available to build", std::move(choices),
        ResolveLayout(m_layout, Style().layouts.topPanel), Style().listSelectorPopup));
}

void BaseView::HandleHurryClicked_()
{
    if (!m_bEditable)
    {
        return;
    }

    const HurryQuote_t quote = m_rBase.QuoteHurry();
    if (!quote.bAvailable || quote.creditCost <= 0)
    {
        return;
    }

    DismissOpenModals_();
    m_elements.push_back(std::make_unique<HurryProductionPopup>(
        ResolveLayout(m_layout, Style().layouts.popupSmall),
        quote.creditCost,
        [this](int credits) { HandleHurryConfirmed_(credits); },
        Style().hurryProductionPopup));
}

void BaseView::HandleHurryConfirmed_(int credits)
{
    if (!m_bEditable || credits <= 0)
    {
        return;
    }

    if (!m_rBase.GetFaction().GetEconomy().CanAfford(credits))
    {
        DismissOpenModals_();
        m_elements.push_back(std::make_unique<NoticePopup>(
            ResolveLayout(m_layout, Style().layouts.popupSmall),
            "Hurry",
            "Not enough energy credits."));
        return;
    }

    m_rBase.HurryProduction(credits);
}

void BaseView::HandleBuildingClicked_(const BuildingConfig_t& rBuilding)
{
    if (!m_bEditable)
    {
        return;
    }

    const BuildingId_t buildingId = rBuilding.id;
    std::vector<PopupChoice_t> choices;
    choices.push_back(
        {"Scrap Building",
         [this, buildingId] { HandleScrapChoice_(buildingId, /*bAllBases*/ false); }});
    choices.push_back(
        {"Scrap this building at all bases",
         [this, buildingId] { HandleScrapChoice_(buildingId, /*bAllBases*/ true); }});

    DismissOpenModals_();
    m_elements.push_back(std::make_unique<ListSelectorPopup>(
        rBuilding.GetName(), "This building cannot be scrapped", std::move(choices),
        ResolveLayout(m_layout, Style().layouts.popupSmall), Style().listSelectorPopup));
}

void BaseView::HandleScrapChoice_(const BuildingId_t& buildingId, bool bAllBases)
{
    if (!m_bEditable)
    {
        return;
    }

    Faction& rFaction = m_rBase.GetFaction();
    const std::optional<ScrapPayout_t> payout =
        bAllBases ? rFaction.QuoteScrapBuildings(buildingId)
                  : rFaction.QuoteScrapBuilding(m_rBase, buildingId);

    if (!payout)
    {
        DismissOpenModals_();
        m_elements.push_back(std::make_unique<NoticePopup>(
            ResolveLayout(m_layout, Style().layouts.popupSmall),
            "Scrap",
            "This building cannot be scrapped."));
        return;
    }

    DismissOpenModals_();
    m_elements.push_back(MakeConfirmPopup(
        FormatScrapConfirm(*payout, rFaction),
        ResolveLayout(m_layout, Style().layouts.popupSmall),
        [this, buildingId, bAllBases] { HandleScrapConfirmed_(buildingId, bAllBases); },
        Style().listSelectorPopup));
}

void BaseView::HandleScrapConfirmed_(const BuildingId_t& buildingId, bool bAllBases)
{
    if (!m_bEditable)
    {
        return;
    }

    Faction& rFaction = m_rBase.GetFaction();
    const std::optional<ScrapPayout_t> payout =
        bAllBases ? rFaction.QuoteScrapBuildings(buildingId)
                  : rFaction.QuoteScrapBuilding(m_rBase, buildingId);
    if (!payout)
    {
        DismissOpenModals_();
        m_elements.push_back(std::make_unique<NoticePopup>(
            ResolveLayout(m_layout, Style().layouts.popupSmall),
            "Scrap",
            "This building cannot be scrapped."));
        return;
    }

    if (bAllBases)
    {
        rFaction.ScrapBuildings(buildingId);
    }
    else
    {
        rFaction.ScrapBuilding(m_rBase, buildingId);
    }
}

} // namespace ac
