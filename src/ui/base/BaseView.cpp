#include "ui/base/BaseView.h"
#include "ui/base/BaseWorkableAreaDisplay.h"
#include "ui/base/GrowthDisplay.h"
#include "ui/base/PopulationDisplay.h"
#include "ui/base/PopTypeSelectorPopup.h"
#include "game/population/pop-types/Pop.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/base/population/PopContainer.h"
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
    GrowthCalculator* pGrowthCalculator,
    WindowLayout_t layout
)
    : IGameView(layout)
    , m_rBase(rBase)
    , m_rFaction(rFaction)
{
    m_elements.push_back(std::make_unique<GrowthDisplay>(
        &m_rBase,
        pGrowthCalculator,
        ResolveLayout(m_layout, k_LeftPanelLayout)
    ));
    m_elements.push_back(std::make_unique<BaseWorkableAreaDisplay>(
        &m_rBase,
        ResolveLayout(m_layout, k_TopPanelLayout),
        [this](int tileX, int tileY) { HandleTileClick_(tileX, tileY); }
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

void BaseView::HandleTileClick_(int tileX, int tileY)
{
    auto& rAssignments = m_rBase.GetWorkerAssignments();
    auto& rPops = m_rBase.GetPopContainer();

    if (rAssignments.IsTileAssigned(tileX, tileY, rPops))
    {
        for (const auto& pPop : rPops.GetPops())
        {
            const TileCoord coord = pPop->GetTileCoord();
            if (pPop->IsWorker() && coord.first == tileX && coord.second == tileY)
            {
                rAssignments.UnassignWorker(*pPop);
                return;
            }
        }
    }
    else
    {
        for (int i = static_cast<int>(rPops.GetPops().size()) - 1; i >= 0; --i)
        {
            Pop* pPop = rPops.GetPops()[i].get();
            const TileCoord coord = pPop->GetTileCoord();
            if (pPop->IsWorker() && coord.first == -1 && coord.second == -1)
            {
                rAssignments.AssignWorker(*pPop, tileX, tileY, rPops);
                return;
            }
        }
    }
}

void BaseView::HandlePopClick(Pop& rPop)
{
    m_elements.push_back(std::make_unique<PopTypeSelectorPopup>(
        m_rFaction.GetAvailablePopTypes(),
        ResolveLayout(m_layout, k_PopupLayoutSmall),
        [this, &rPop](const PopTypeConfig& rConfig) {
            HandlePopTypeSelected(rPop, rConfig);
        }
    ));
}

void BaseView::HandlePopTypeSelected(Pop& rPop, const PopTypeConfig& rConfig)
{
    m_rBase.ConvertPop(rPop, rConfig.id);
}

} // namespace ac
