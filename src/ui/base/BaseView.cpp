#include "ui/base/BaseView.h"
#include "ui/base/BaseWorkableAreaDisplay.h"
#include "ui/base/PopulationDisplay.h"
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
    Graphics& rGraphics
)
{
    WindowLayout_t window = {static_cast<int>(rGraphics.getSize().x), static_cast<int>(rGraphics.getSize().y)};
    m_elements.push_back(std::make_unique<BaseWorkableAreaDisplay>(rGraphics, rBase, Resolve(k_WorkableAreaLayout, window)));
    m_elements.push_back(std::make_unique<PopulationDisplay>(rGraphics, &rBase.GetPopContainer(), Resolve(k_TopPanelLayout, window)));
}

BaseView::~BaseView() = default;


void BaseView::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
    }
}
} // namespace ac
