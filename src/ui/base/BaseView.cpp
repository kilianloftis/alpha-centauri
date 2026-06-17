#include "ui/base/BaseView.h"
#include "ui/base/BaseDisplay.h"
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
    const WorldMap& rWorldMap,
    EventBus& rBus,
    Graphics& rGraphics,
    UIManager& rUIManager
)
: m_rBase(rBase)
, m_pWorkableAreaDisplay(std::make_unique<BaseWorkableAreaDisplay>(rGraphics, rWorldMap))
, m_rUIManager(rUIManager)
, m_pBaseDisplay(std::make_unique<BaseDisplay>(rBase, rGraphics))
, m_pPopDisplay(std::make_unique<PopulationDisplay>(rBus, rGraphics))
{
    m_pWorkableAreaDisplay->SetBase(&rBase);
    m_pPopDisplay->SetCurrentPop(rBase.GetBaseSize());
    m_pPopDisplay->SetPopulation(&rBase.GetPopContainer());
    m_pPopDisplay->SetRenderPosition(620.f, 40.f);
    m_panels.push_back(m_pWorkableAreaDisplay.get());
    m_panels.push_back(m_pBaseDisplay.get());
    m_panels.push_back(m_pPopDisplay.get());
}

BaseView::~BaseView() = default;

void BaseView::OnPopped()
{
    m_pWorkableAreaDisplay->SetBase(nullptr);
    m_pPopDisplay->SetPopulation(nullptr);
}

void BaseView::Render(Graphics& rGraphics)
{
    rGraphics.DrawFilledRect(0.f, 0.f, kScreenWidth, kScreenHeight, Color{20, 20, 30, 255});
    for (IBasePanel* pPanel : m_panels)
    {
        pPanel->Render(rGraphics);
    }
}

void BaseView::Update(float /*deltaTime*/)
{
}

void BaseView::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        m_rUIManager.PopView();
    }
}

void BaseView::HandleMouse(const MouseEvent_t& rEvent)
{
    auto tile = TileHitTester::HitTestBaseWorkableArea(
        static_cast<float>(rEvent.x), static_cast<float>(rEvent.y),
        BaseWorkableAreaDisplay::kBaseAreaCenterX,
        BaseWorkableAreaDisplay::kBaseAreaCenterY,
        BaseWorkableAreaDisplay::kBaseTileSize,
        m_rBase.GetX(), m_rBase.GetY());

    if (!tile)
    {
        m_lastClickedTile = std::nullopt;
        m_pBaseDisplay->SetLastClickedTileText("Clicked: (" + std::to_string(rEvent.x) + ", " + std::to_string(rEvent.y) + ") - no tile");
        return;
    }

    m_lastClickedTile = tile;
    int tileX = tile->first;
    int tileY = tile->second;
    auto& rAssignments = m_rBase.GetWorkerAssignments();
    const auto& rPops = m_rBase.GetPopContainer();

    if (rAssignments.IsTileAssigned(tileX, tileY))
    {
        for (const auto& rEntry : rAssignments.GetAssignments())
        {
            if (rEntry.second.first == tileX && rEntry.second.second == tileY)
            {
                rAssignments.UnassignWorker(rEntry.first);
                m_pBaseDisplay->SetLastClickedTileText("Unassigned worker from (" + std::to_string(tileX) + ", " + std::to_string(tileY) + ")");
                return;
            }
        }
    }
    else
    {
        const auto& rPopsVec = rPops.GetPops();
        for (int i = static_cast<int>(rPopsVec.size()) - 1; i >= 0; --i)
        {
            const Pop* pPop = rPopsVec[i].get();
            if (pPop->IsWorker() && rAssignments.GetAssignedTile(pPop->GetId()).first == -1)
            {
                rAssignments.UnassignWorker(pPop->GetId());
                if (rAssignments.AssignWorker(pPop->GetId(), tileX, tileY, rPops))
                {
                    m_pBaseDisplay->SetLastClickedTileText("Reassigned worker to (" + std::to_string(tileX) + ", " + std::to_string(tileY) + ")");
                    return;
                }
            }
        }
        m_pBaseDisplay->SetLastClickedTileText("No workers available to reassign");
    }
}

} // namespace ac
