#include "ui/world/UnitOrderInputController.h"

#include "game/map/Tile.h"

namespace ac
{

bool UnitOrderInputController::HandleKey(const KeyEvent_t& rEvent, Unit* pSelectedUnit)
{
    if (!pSelectedUnit)
    {
        return false;
    }

    auto it = m_orderHandlers.find(rEvent.key);
    if (it == m_orderHandlers.end())
    {
        return false;
    }

    it->second(*pSelectedUnit);
    return true;
}

bool UnitOrderInputController::HandleMouse(const MouseEvent_t& rEvent, Unit* pSelectedUnit,
                                           const Tile* pHoveredTile,
                                           const Pathfinder* pPathfinder)
{
    // --- Left-click: long-press path preview + move order ---
    if (rEvent.button == MouseButton_t::Left)
    {
        if (rEvent.bPressed)
        {
            if (!pSelectedUnit)
            {
                return false;
            }
            m_bLeftButtonHeld = true;
            m_leftButtonPressTime = std::chrono::steady_clock::now();
            m_pPreviewUnit = pSelectedUnit;
            m_pPreviewDestination = pHoveredTile;
            m_bPreviewActive = false;
            return true;
        }

        // Release.
        if (!m_bLeftButtonHeld)
        {
            return false;
        }

        m_bLeftButtonHeld = false;

        if (m_bPreviewActive && m_pPreviewUnit && m_pathPreview.bReachable)
        {
            m_pPreviewUnit->SetOrder(MoveOrder_t{m_pPreviewDestination});
            CancelPreview();
            return true;
        }

        CancelPreview();
        return false;
    }

    // --- Mouse move: update preview destination while holding ---
    if (rEvent.button == MouseButton_t::None && m_bLeftButtonHeld && pSelectedUnit && pPathfinder)
    {
        const auto elapsed = std::chrono::steady_clock::now() - m_leftButtonPressTime;
        if (elapsed < std::chrono::milliseconds(k_holdThresholdMs))
        {
            return false;
        }

        if (pHoveredTile && pHoveredTile != &pSelectedUnit->GetTile())
        {
            m_pPreviewUnit = pSelectedUnit;
            if (pHoveredTile != m_pPreviewDestination)
            {
                m_pPreviewDestination = pHoveredTile;
                UpdatePreview_(*pSelectedUnit, *pHoveredTile, *pPathfinder);
            }
            else if (!m_bPreviewActive)
            {
                UpdatePreview_(*pSelectedUnit, *pHoveredTile, *pPathfinder);
            }
        }
        else
        {
            m_bPreviewActive = false;
        }

        return false;
    }

    return false;
}

void UnitOrderInputController::UpdatePreview_(Unit& rMover, const Tile& rDestination,
                                              const Pathfinder& rPathfinder)
{
    m_pathPreview = rPathfinder.FindPath(rMover, rDestination);
    m_bPreviewActive = m_pathPreview.bReachable && !m_pathPreview.tiles.empty();
}

const Path_t* UnitOrderInputController::GetPathPreview() const
{
    if (m_bPreviewActive)
    {
        return &m_pathPreview;
    }
    return nullptr;
}

void UnitOrderInputController::CancelPreview()
{
    m_bPreviewActive = false;
    m_pPreviewUnit = nullptr;
    m_pPreviewDestination = nullptr;
    m_pathPreview = {};
}

} // namespace ac
