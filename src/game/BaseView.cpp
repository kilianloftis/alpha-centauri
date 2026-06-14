#include "game/BaseView.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/base/population/PopContainer.h"
#include "ui/BaseWorkableAreaDisplay.h"
#include "ui/UIManager.h"
#include "ui/TileHitTester.h"
#include "graphics/Graphics.h"
#include <string>

namespace ac
{

BaseView::BaseView(
    BaseManager& rBase,
    BaseWorkableAreaDisplay& rWorkableAreaDisplay,
    UIManager& rUIManager
)
: m_rBase(rBase)
, m_rWorkableAreaDisplay(rWorkableAreaDisplay)
, m_rUIManager(rUIManager)
{
    m_rWorkableAreaDisplay.SetBase(&rBase);
}

void BaseView::OnPopped()
{
    m_rWorkableAreaDisplay.SetBase(nullptr);
}

void BaseView::Render(Graphics& rGraphics)
{
    rGraphics.DrawText(m_rBase.GetName(), 20.f, 40.f, 20, Color::Yellow());
    rGraphics.DrawText("Nutrients: " + std::to_string(m_rBase.GetNutrientStockpile()), 20.f, 70.f, 16, Color::White());
    rGraphics.DrawText("Minerals:  " + std::to_string(m_rBase.GetMineralStockpile()), 20.f, 90.f, 16, Color::White());
    rGraphics.DrawText("Energy:    " + std::to_string(m_rBase.GetEnergyProduction()) + "/turn", 20.f, 110.f, 16, Color::White());
    m_rWorkableAreaDisplay.Render(kBaseAreaCenterX, kBaseAreaCenterY, kBaseTileSize);
    if (!m_lastClickedTileText.empty())
    {
        rGraphics.DrawText(m_lastClickedTileText, 20.f, 570.f, 18, Color::Yellow());
    }
}

void BaseView::Update(float /*deltaTime*/)
{
}

bool BaseView::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        m_rUIManager.PopView();
        return true;
    }
    return false;
}

bool BaseView::HandleMouse(const MouseEvent_t& rEvent)
{
    auto tile = TileHitTester::HitTestBaseWorkableArea(
        static_cast<float>(rEvent.x), static_cast<float>(rEvent.y),
        kBaseAreaCenterX, kBaseAreaCenterY, kBaseTileSize,
        m_rBase.GetX(), m_rBase.GetY());

    if (!tile)
    {
        m_lastClickedTile = std::nullopt;
        m_lastClickedTileText = "Clicked: (" + std::to_string(rEvent.x) + ", " + std::to_string(rEvent.y) + ") - no tile";
        return false;
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
                m_lastClickedTileText = "Unassigned worker from (" + std::to_string(tileX) + ", " + std::to_string(tileY) + ")";
                return true;
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
                    m_lastClickedTileText = "Reassigned worker to (" + std::to_string(tileX) + ", " + std::to_string(tileY) + ")";
                    return true;
                }
            }
        }
        m_lastClickedTileText = "No workers available to reassign";
    }
    return true;
}

std::vector<UIElement*> BaseView::GetElements()
{
    return {};
}

} // namespace ac
