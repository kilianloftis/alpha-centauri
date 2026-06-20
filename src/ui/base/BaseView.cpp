#include "ui/base/BaseView.h"
#include "ui/base/BaseWorkableAreaDisplay.h"
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
    WindowLayout_t layout
)
    : IGameView(layout)
    , m_rBase(rBase)
    , m_rFaction(rFaction)
{
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
    const auto& rPops = m_rBase.GetPopContainer();

    if (rAssignments.IsTileAssigned(tileX, tileY))
    {
        for (const auto& rEntry : rAssignments.GetAssignments())
        {
            if (rEntry.second.first == tileX && rEntry.second.second == tileY)
            {
                rAssignments.UnassignWorker(rEntry.first);
                return;
            }
        }
    }
    else
    {
        for (int i = static_cast<int>(rPops.GetPops().size()) - 1; i >= 0; --i)
        {
            const Pop* pPop = rPops.GetPops()[i].get();
            if (pPop->IsWorker() && rAssignments.GetAssignedTile(pPop->GetId()).first == -1)
            {
                rAssignments.AssignWorker(pPop->GetId(), tileX, tileY, rPops);
                return;
            }
        }
    }
}

void BaseView::HandlePopClick(Pop& rPop)
{
    m_elements.push_back(std::make_unique<PopTypeSelectorPopup>(
        m_rFaction,
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
