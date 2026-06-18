#include "ui/base/BaseView.h"
#include "ui/base/BaseWorkableAreaDisplay.h"
#include "ui/base/PopulationDisplay.h"
#include "game/population/pop-types/Pop.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/base/population/PopContainer.h"
#include "game/map/WorldMap.h"
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
)
    : UIGroup(WindowLayout_t{0, 0, 0, 0})  // Layout will be resolved from ratios in Update
{
    m_elements.push_back(std::make_unique<BaseWorkableAreaDisplay>(&rBase, WindowLayout_t{0, 0, 0, 0}));
    m_elements.push_back(std::make_unique<PopulationDisplay>(&rBase.GetPopContainer(), k_BottomPanelLayout, std::bind(&BaseView::HandlePopClick, this, std::placeholders::_1)));
}

BaseView::~BaseView() = default;

void BaseView::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
    }
}

void BaseView::HandlePopClick(const Pop& rPop)
{

}

} // namespace ac
