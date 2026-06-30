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
    const ImprovementRegistry& rImprovements,
    WindowLayout_t layout
)
    : IGameView(layout)
    , m_rBase(rBase)
    , m_rFaction(rFaction)
{
    m_elements.push_back(std::make_unique<GrowthDisplay>(
        &m_rBase,
        ResolveLayout(m_layout, k_LeftPanelLayout)
    ));
    m_elements.push_back(std::make_unique<BaseWorkableAreaDisplay>(
        &m_rBase,
        rImprovements,
        ResolveLayout(m_layout, k_TopPanelLayout),
        [this](const Tile* pTile) { HandleTileClick_(pTile); },
        [this]() { HandleBaseClicked_(); }
    ));
    m_elements.push_back(std::make_unique<ProductionDisplay>(
        &m_rBase,
        ResolveLayout(m_layout, k_BottomLeftPanelLayout),
        [this]() { HandleProductionDisplayClicked_(); }
    ));
    m_elements.push_back(std::make_unique<PopulationDisplay>(
        &m_rBase.GetPopContainer(),
        ResolveLayout(m_layout, k_CenterPanelLayout),
        [this](Pop& rPop) { HandlePopClick(rPop); }
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
    auto& rAssignments = m_rBase.GetWorkerAssignments();
    rAssignments.ResetAllAssignments();
}

void BaseView::HandleTileClick_(const Tile* pTile)
{
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
    m_rBase.ConvertPop(rPop, rConfig.id);
}

void BaseView::HandleProductionDisplayClicked_()
{
    std::vector<const IConstructable*> available = m_rBase.GetConstructable();

    m_elements.push_back(std::make_unique<ProductionSelectorPopup>(
        std::move(available),
        ResolveLayout(m_layout, k_TopPanelLayout),
        [this](const IConstructable& rItem) { m_rBase.SetProduction(&rItem); }
    ));
}

} // namespace ac
