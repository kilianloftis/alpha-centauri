#include "ui/base/BaseView.h"
#include "ui/base/BaseWorkableAreaDisplay.h"
#include "ui/base/PopulationDisplay.h"
#include "ui/base/PopTypeSelectorPopup.h"
#include "game/population/pop-types/Pop.h"
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
    const WorldMap& /*rWorldMap*/,
    const Faction& rFaction,
    WindowLayout_t layout
)
    : UIGroup(layout)
    , m_rBase(rBase)
    , m_rFaction(rFaction)
{
    m_elements.push_back(std::make_unique<BaseWorkableAreaDisplay>(&m_rBase, ResolveLayout(m_layout, k_WorkableAreaLayout)));
    m_elements.push_back(std::make_unique<PopulationDisplay>(
        &m_rBase.GetPopContainer(),
        ResolveLayout(m_layout, k_BottomPanelLayout),
        [this](Pop& rPop) { HandlePopClick(rPop); }
    ));
}

BaseView::~BaseView() = default;

void BaseView::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
    }
}

void BaseView::HandlePopClick(Pop& rPop)
{
    m_elements.push_back(std::make_unique<PopTypeSelectorPopup>(
        m_rFaction,
        ResolveLayout(m_layout, k_PopupLayout),
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
