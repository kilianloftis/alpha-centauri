#include "ui/base/BaseView.h"
#include "ui/base/BaseNameDisplay.h"
#include "ui/base/BaseWorkableAreaDisplay.h"
#include "ui/base/GrowthDisplay.h"
#include "ui/base/ProductionDisplay.h"
#include "ui/base/ProductionSelectorPopup.h"
#include "ui/base/PopulationDisplay.h"
#include "ui/base/PopTypeSelectorPopup.h"
#include "ui/base/SupportDisplay.h"
#include "ui/PlaceholderPanel.h"
#include "ui/world/UnitStackPanel.h"
#include "game/population/pop-types/Pop.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/Faction.h"
#include "ui/style/UiStyle.h"
#include "graphics/Graphics.h"
#include <string>

namespace ac
{

BaseView::BaseView(
    BaseManager& rBase,
    const Faction& rFaction,
    WindowLayout_t layout,
    bool bEditable
)
    : IGameView(layout)
    , m_rBase(rBase)
    , m_rFaction(rFaction)
    , m_bEditable(bEditable)
{
    const auto& bv = Style().baseView;
    const WindowLayout_t topPanel = ResolveLayout(m_layout, Style().layouts.topPanel);
    const WindowLayout_t leftPanel = ResolveLayout(m_layout, Style().layouts.leftPanel);
    const WindowLayout_t centerPanel = ResolveLayout(m_layout, Style().layouts.centerPanel);

    BaseWorkableAreaDisplay::TileClickCallback_t onTileClick;
    BaseWorkableAreaDisplay::BaseClickCallback_t onBaseClick;
    std::function<void()> onProductionClick;
    PopClickCallback_t onPopClick;
    if (m_bEditable)
    {
        onTileClick = [this](const Tile* pTile) { HandleTileClick_(pTile); };
        onBaseClick = [this]() { HandleBaseClicked_(); };
        onProductionClick = [this]() { HandleProductionDisplayClicked_(); };
        onPopClick = [this](Pop& rPop) { HandlePopClick(rPop); };
    }

    // TopPanel: Growth | Workable (60%) | Buildings
    m_elements.push_back(std::make_unique<GrowthDisplay>(
        &m_rBase,
        ResolveLayout(topPanel, bv.growthLayout)
    ));
    m_elements.push_back(std::make_unique<BaseWorkableAreaDisplay>(
        &m_rBase,
        ResolveLayout(topPanel, bv.workableLayout),
        std::move(onTileClick),
        std::move(onBaseClick)
    ));
    m_elements.push_back(std::make_unique<PlaceholderPanel>(
        "Buildings",
        ResolveLayout(topPanel, bv.buildingsLayout)
    ));

    // LeftPanel: Production (2/3) | Build Queue (1/3)
    m_elements.push_back(std::make_unique<ProductionDisplay>(
        &m_rBase,
        ResolveLayout(leftPanel, bv.productionLayout),
        std::move(onProductionClick)
    ));
    m_elements.push_back(std::make_unique<PlaceholderPanel>(
        "Build Queue",
        ResolveLayout(leftPanel, bv.buildQueueLayout)
    ));

    // CenterPanel: Base Name (1/3) | Population (2/3)
    m_elements.push_back(std::make_unique<BaseNameDisplay>(
        &m_rBase,
        ResolveLayout(centerPanel, bv.baseNameLayout)
    ));
    m_elements.push_back(std::make_unique<PopulationDisplay>(
        &m_rBase.GetPopulation(),
        ResolveLayout(centerPanel, bv.populationLayout),
        std::move(onPopClick)
    ));

    // RightPanel: Support
    m_elements.push_back(std::make_unique<SupportDisplay>(
        &m_rBase,
        ResolveLayout(m_layout, Style().layouts.rightPanel)
    ));

    // BottomPanel: Unit stack (same component / slot as WorldView)
    auto pUnitStack = std::make_unique<UnitStackPanel>(
        ResolveLayout(m_layout, Style().layouts.bottomPanel),
        [this](Unit& rUnit) { HandleUnitStackClicked_(rUnit); });
    m_pUnitStackPanel = pUnitStack.get();
    m_elements.push_back(std::move(pUnitStack));
}

BaseView::~BaseView() = default;

void BaseView::Render(Graphics& rGraphics)
{
    RefreshUnitStack_();
    IGameView::Render(rGraphics);
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

    if (rAssignments.IsTileAssigned(pTile))
    {
        rAssignments.UserUnassignTile(pTile);
    }
    else
    {
        m_rBase.UserAssignBestAvailableWorker(pTile);
    }
}

void BaseView::HandlePopClick(Pop& rPop)
{
    if (!m_bEditable)
    {
        return;
    }

    DismissOpenModals_();
    m_elements.push_back(std::make_unique<PopTypeSelectorPopup>(
        m_rFaction.GetAvailablePopTypes(),
        ResolveLayout(m_layout, Style().layouts.popupSmall),
        [this, &rPop](const PopTypeConfig_t& rConfig) {
            HandlePopTypeSelected(rPop, rConfig);
        }
    ));
}

void BaseView::HandlePopTypeSelected(Pop& rPop, const PopTypeConfig_t& rConfig)
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

    std::vector<const IConstructable*> available = m_rBase.GetConstructable();

    DismissOpenModals_();
    m_elements.push_back(std::make_unique<ProductionSelectorPopup>(
        std::move(available),
        ResolveLayout(m_layout, Style().layouts.topPanel),
        [this](const IConstructable& rItem) { m_rBase.GetProduction().SetProduction(&rItem); }
    ));
}

} // namespace ac
