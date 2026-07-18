#include "ui/base/BaseView.h"
#include "ui/base/BaseWorkableAreaDisplay.h"
#include "ui/base/GrowthDisplay.h"
#include "ui/base/ProductionDisplay.h"
#include "ui/base/ProductionSelectorPopup.h"
#include "ui/base/PopulationDisplay.h"
#include "ui/base/PopTypeSelectorPopup.h"
#include "game/population/pop-types/Pop.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/map/WorldMap.h"
#include "game/Faction.h"
#include "lib/EventBus.h"
#include "ui/UIManager.h"
#include "ui/TileHitTester.h"
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
    const WindowLayout_t leftColumn = ResolveLayout(m_layout, {
        k_LeftPanelLayout.x,
        0.25f,
        k_LeftPanelLayout.width,
        2.0f * k_LeftPanelLayout.height
    });

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

    m_elements.push_back(std::make_unique<GrowthDisplay>(
        &m_rBase,
        ResolveLayout(leftColumn, {0.0f, 0.0f, 1.0f, 0.5f})
    ));
    m_elements.push_back(std::make_unique<BaseWorkableAreaDisplay>(
        &m_rBase,
        ResolveLayout(m_layout, k_TopPanelLayout),
        std::move(onTileClick),
        std::move(onBaseClick)
    ));
    m_elements.push_back(std::make_unique<ProductionDisplay>(
        &m_rBase,
        ResolveLayout(leftColumn, {0.0f, 0.5f, 1.0f, 0.5f}),
        std::move(onProductionClick)
    ));
    m_elements.push_back(std::make_unique<PopulationDisplay>(
        &m_rBase.GetPopulation(),
        ResolveLayout(m_layout, k_BottomPanelLayout),
        std::move(onPopClick)
    ));
}

BaseView::~BaseView() = default;

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

    m_elements.push_back(std::make_unique<PopTypeSelectorPopup>(
        m_rFaction.GetAvailablePopTypes(),
        ResolveLayout(m_layout, k_PopupLayoutSmall),
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

    m_elements.push_back(std::make_unique<ProductionSelectorPopup>(
        std::move(available),
        ResolveLayout(m_layout, k_TopPanelLayout),
        [this](const IConstructable& rItem) { m_rBase.GetProduction().SetProduction(&rItem); }
    ));
}

} // namespace ac
