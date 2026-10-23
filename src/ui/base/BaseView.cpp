#include "ui/base/BaseView.h"
#include "ui/base/BaseNameDisplay.h"
#include "ui/base/BaseWorkableAreaDisplay.h"
#include "ui/base/BuildQueueDisplay.h"
#include "ui/base/BuildingsDisplay.h"
#include "ui/base/CommerceDisplay.h"
#include "ui/base/GrowthDisplay.h"
#include "ui/base/HurryProductionPopup.h"
#include "ui/base/ProductionDisplay.h"
#include "ui/base/PopulationDisplay.h"
#include "ui/base/SupportDisplay.h"
#include "ui/ConfirmPopup.h"
#include "ui/ListSelectorPopup.h"
#include "ui/NoticePopup.h"
#include "ui/ScrapRefundText.h"
#include "ui/SpriteLibrary.h"
#include "ui/world/UnitStackPanel.h"
#include "game/population/Pop.h"
#include "game/population/PopTypeConfigParser.h"
#include "game/base/BaseManager.h"
#include "game/buildings/BuildingManager.h"
#include "game/population/PopulationManager.h"
#include "game/production/HurryProductionCalculator.h"
#include "game/production/ScrapConfig.h"
#include "game/production/ProductionRules.h"
#include "game/production/ProductionManager.h"
#include "game/economy/WorkerAssignmentManager.h"
#include "game/economy/EconomyManager.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/faction/Faction.h"
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
    MapRenderer& rMapRenderer,
    SpriteLibrary& rSprites
)
    : IGameView(layout)
    , m_rBase(rBase)
    , m_pOwnerAtOpen(&rBase.GetFaction())
    , m_bEditable(rBase.GetFaction().IsPlayerControlled())
    , m_rSprites(rSprites)
    , m_destroyedConnection(rBase.OnDestroyed.ConnectScoped([this]() { m_bShouldClose = true; }))
{
    m_rBase.GetPopulation().EnsureCompositionCurrent();
    m_snapshot = BuildBaseDisplaySnapshot(m_rBase);

    BuildTopPanel_(rMapRenderer);
    BuildLeftPanel_();
    BuildCenterPanel_();
    m_elements.push_back(std::make_unique<SupportDisplay>(
        m_rBase,
        ResolveLayout(m_layout, Style().layouts.rightPanel)
    ));
    BuildUnitStack_();
}

void BaseView::BuildTopPanel_(MapRenderer& rMapRenderer)
{
    const auto& bv = Style().baseView;
    const WindowLayout_t topPanel = ResolveLayout(m_layout, Style().layouts.topPanel);

    BaseWorkableAreaDisplay::TileClickCallback_t onTileClick;
    BaseWorkableAreaDisplay::BaseClickCallback_t onBaseClick;
    BuildingClickCallback_t onBuildingClick;
    if (m_bEditable)
    {
        onTileClick = [this](const Tile* pTile) { HandleTileClick_(pTile); };
        onBaseClick = [this]() { HandleBaseClicked_(); };
        onBuildingClick = [this](const BuildingConfig_t& rBuilding)
        { HandleBuildingClicked_(rBuilding); };
    }

    // Growth + Commerce | Workable (60%) | Buildings
    m_elements.push_back(std::make_unique<GrowthDisplay>(
        m_rBase,
        m_snapshot,
        ResolveLayout(topPanel, bv.growthLayout)
    ));
    m_elements.push_back(std::make_unique<CommerceDisplay>(
        m_rBase,
        ResolveLayout(topPanel, bv.commerceLayout)
    ));
    m_elements.push_back(std::make_unique<BaseWorkableAreaDisplay>(
        m_rBase,
        m_snapshot,
        ResolveLayout(topPanel, bv.workableLayout),
        rMapRenderer,
        std::move(onTileClick),
        std::move(onBaseClick)
    ));
    m_elements.push_back(std::make_unique<BuildingsDisplay>(
        m_rBase,
        m_rSprites,
        ResolveLayout(topPanel, bv.buildingsLayout),
        std::move(onBuildingClick)
    ));
}

void BaseView::BuildLeftPanel_()
{
    const auto& bv = Style().baseView;
    const WindowLayout_t leftPanel = ResolveLayout(m_layout, Style().layouts.leftPanel);

    std::function<void()> onProductionClick;
    std::function<void()> onHurryClick;
    if (m_bEditable)
    {
        onProductionClick = [this]() { HandleProductionDisplayClicked_(); };
        onHurryClick = [this]() { HandleHurryClicked_(); };
    }

    // Production (2/3) | Build Queue (1/3)
    m_elements.push_back(std::make_unique<ProductionDisplay>(
        m_rBase,
        m_snapshot,
        m_rSprites,
        ResolveLayout(leftPanel, bv.productionLayout),
        std::move(onProductionClick)
    ));
    m_elements.push_back(std::make_unique<BuildQueueDisplay>(
        m_rBase,
        ResolveLayout(leftPanel, bv.buildQueueLayout),
        std::move(onHurryClick)
    ));
}

void BaseView::BuildCenterPanel_()
{
    const auto& bv = Style().baseView;
    const WindowLayout_t centerPanel = ResolveLayout(m_layout, Style().layouts.centerPanel);

    PopClickCallback_t onPopClick;
    if (m_bEditable)
    {
        onPopClick = [this](Pop& rPop) { HandlePopClick_(rPop); };
    }

    // Base Name (1/3) | Population (2/3)
    m_elements.push_back(std::make_unique<BaseNameDisplay>(
        m_rBase,
        ResolveLayout(centerPanel, bv.baseNameLayout)
    ));
    m_elements.push_back(std::make_unique<PopulationDisplay>(
        m_rBase.GetPopulation(),
        ResolveLayout(centerPanel, bv.populationLayout),
        std::move(onPopClick)
    ));
}

void BaseView::BuildUnitStack_()
{
    // Same component and slot as WorldView's.
    auto pUnitStack = std::make_unique<UnitStackPanel>(
        ResolveLayout(m_layout, Style().layouts.bottomPanel), m_rSprites,
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
    // A transfer keeps the base alive (destruction is m_destroyedConnection's), so close rather
    // than show a base that changed hands.
    if (&m_rBase.GetFaction() != m_pOwnerAtOpen)
    {
        m_bShouldClose = true;
        return;
    }

    m_rBase.GetPopulation().EnsureCompositionCurrent();

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
    for (Unit* pUnit : rMap.GetAllUnitsOnTile(rTile))
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
    auto& rAssignments = m_rBase.GetWorkerAssignments();
    rAssignments.ResetAllAssignments();
}

void BaseView::HandleTileClick_(const Tile* pTile)
{
    auto& rAssignments = m_rBase.GetWorkerAssignments();

    // Toggles against what this base does; assigning a tile someone else works finds no free pop
    // and fails.
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
    std::vector<PopupChoice_t> choices;
    for (const PopTypeConfig_t* pConfig : m_rBase.GetFaction().GetAvailablePopTypes())
    {
        choices.push_back(
            {pConfig->name, [this, &rPop, pConfig] { m_rBase.ConvertPop(rPop, pConfig->id); }});
    }

    DismissOpenModals_();
    auto pPopup = std::make_unique<ListSelectorPopup>(
        "Select Pop Type", "No pop types available", std::move(choices),
        GetPopupLayout(), Style().listSelectorPopup, &m_rSprites);
    m_elements.push_back(std::move(pPopup));
}

void BaseView::HandleProductionDisplayClicked_()
{
    std::vector<PopupChoice_t> choices;
    const std::vector<const IConstructable*> constructable = ConstructableItems(m_rBase);
    for (const IConstructable* pItem : constructable)
    {
        PopupChoice_t choice;
        choice.label = pItem->GetName();
        choice.onChosen = [this, pItem]
        {
            m_rBase.GetProduction().SetProduction(pItem, m_rBase.GetBaseEffects());
        };
        if (const auto* pBuilding = dynamic_cast<const BuildingConfig_t*>(pItem))
        {
            choice.iconPath = pBuilding->icon;
        }
        choices.push_back(std::move(choice));
    }

    DismissOpenModals_();
    auto pPopup = std::make_unique<ListSelectorPopup>(
        "Select Production", "Nothing available to build", std::move(choices),
        ResolveLayout(m_layout, Style().layouts.topPanel), Style().listSelectorPopup,
        &m_rSprites);
    m_elements.push_back(std::move(pPopup));
}

void BaseView::HandleHurryClicked_()
{
    const HurryQuote_t quote = m_rBase.QuoteHurry();
    if (!quote.bAvailable || quote.creditCost <= 0)
    {
        return;
    }

    DismissOpenModals_();
    auto pPopup = std::make_unique<HurryProductionPopup>(
        GetPopupLayout(),
        quote.creditCost,
        [this](int credits) { HandleHurryConfirmed_(credits); },
        Style().hurryProductionPopup);
    m_elements.push_back(std::move(pPopup));
}

void BaseView::HandleHurryConfirmed_(int credits)
{
    if (credits <= 0)
    {
        return;
    }

    if (!m_rBase.GetFaction().GetEconomy().CanAfford(credits))
    {
        ShowNotice_("Hurry", "Not enough energy credits.");
        return;
    }

    m_rBase.HurryProduction(credits);
}

void BaseView::HandleBuildingClicked_(const BuildingConfig_t& rBuilding)
{
    const BuildingId_t buildingId = rBuilding.id;
    std::vector<PopupChoice_t> choices;
    choices.push_back({"Scrap Building", [this, buildingId] { OfferScrapHere_(buildingId); }});
    choices.push_back({"Scrap this building at all bases",
                       [this, buildingId] { OfferScrapEverywhere_(buildingId); }});

    DismissOpenModals_();
    auto pPopup = std::make_unique<ListSelectorPopup>(
        rBuilding.GetName(), "This building cannot be scrapped", std::move(choices),
        GetPopupLayout(), Style().listSelectorPopup, &m_rSprites);
    m_elements.push_back(std::move(pPopup));
}

void BaseView::OfferScrapHere_(const BuildingId_t& buildingId)
{
    const Faction& rFaction = m_rBase.GetFaction();
    const std::optional<ScrapPayout_t> payout = rFaction.QuoteScrapBuilding(m_rBase, buildingId);
    ConfirmScrap_(payout, [this, buildingId]
    {
        m_rBase.GetFaction().ScrapBuilding(m_rBase, buildingId);
    });
}

void BaseView::OfferScrapEverywhere_(const BuildingId_t& buildingId)
{
    const Faction& rFaction = m_rBase.GetFaction();
    const std::optional<ScrapPayout_t> payout = rFaction.QuoteScrapBuildings(buildingId);
    ConfirmScrap_(payout, [this, buildingId]
    {
        m_rBase.GetFaction().ScrapBuildings(buildingId);
    });
}

void BaseView::ConfirmScrap_(const std::optional<ScrapPayout_t>& payout,
                             std::function<void()> onConfirmed)
{
    if (!payout)
    {
        ShowNotice_("Scrap", "This building cannot be scrapped.");
        return;
    }

    // The popup is modal, so the turn cannot advance and change the quote before it is confirmed.
    DismissOpenModals_();
    const std::string message = FormatScrapConfirm(*payout, m_rBase.GetFaction());
    auto pPopup = MakeConfirmPopup(
        message, GetPopupLayout(), std::move(onConfirmed), Style().listSelectorPopup);
    m_elements.push_back(std::move(pPopup));
}

void BaseView::ShowNotice_(const std::string& rTitle, const std::string& rMessage)
{
    DismissOpenModals_();
    auto pPopup = std::make_unique<NoticePopup>(
        GetPopupLayout(), rTitle, rMessage, nullptr, &m_rSprites);
    m_elements.push_back(std::move(pPopup));
}

} // namespace ac
