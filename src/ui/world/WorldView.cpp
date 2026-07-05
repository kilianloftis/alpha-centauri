#include "ui/world/WorldView.h"
#include <algorithm>
#include "ui/world/InfoPanelElement.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"
#include "ui/TileHitTester.h"
#include "graphics/Graphics.h"
#include <string>
#include <memory>

namespace ac
{

WorldView::WorldView(
    GameState& rGameState,
    const WorldMap& rWorldMap,
    WindowLayout_t layout,
    std::function<void()> onProcessTurn,
    std::function<void()> onRequestExit,
    OpenBaseCallback_t onOpenBase
)
: IGameView(layout)
, m_rGameState(rGameState)
, m_mapLayout(ResolveLayout(layout, k_MapLayout))
, m_pWorldDisplay(std::make_unique<WorldDisplay>(m_mapLayout))
, m_onProcessTurn(std::move(onProcessTurn))
, m_onRequestExit(std::move(onRequestExit))
, m_onOpenBase(std::move(onOpenBase))
, m_pCameraInputController(std::make_unique<CameraInputController>(*m_pWorldDisplay, rWorldMap, m_mapLayout))
, m_pUnitOrderInputController(std::make_unique<UnitOrderInputController>())
{
    m_pWorldDisplay->SetWorldMap(&rWorldMap);

    m_elements.push_back(std::make_unique<InfoPanelElement>(ResolveLayout(m_layout, k_InfoPanelLayout)));
}

void WorldView::Render(Graphics& rGraphics)
{
    Update_();
    m_pWorldDisplay->Render(rGraphics);
    IGameView::Render(rGraphics);
}

void WorldView::Update_()
{
    std::vector<InfoPanelElement::InfoLine> infoLines;
    infoLines.push_back({"Mission Year: " + std::to_string(m_rGameState.GetMissionYear()), Color::White()});
    const auto& rFactions = m_rGameState.GetFactions();
    if (!rFactions.empty())
    {
        const Faction* pPlayerFaction = rFactions[0].get();
        infoLines.push_back({"Energy: " + std::to_string(pPlayerFaction->GetEnergy()), Color::Yellow()});
        infoLines.push_back({"Research: " + std::to_string(pPlayerFaction->GetResearchPoints()), Color{100, 200, 255, 255}});
    }
    static_cast<InfoPanelElement*>(m_elements[1].get())->SetInfoLines(infoLines);

    std::vector<BaseInfo_t> baseInfo;
    for (const auto& pFaction : m_rGameState.GetFactions())
    {
        for (const auto& pBase : pFaction->GetBases())
        {
            if (pBase)
            {
                // TODO: Track previousFactionId when base capture is implemented
                baseInfo.push_back({
                    pBase->GetX(),
                    pBase->GetY(),
                    pBase->GetName(),
                    pBase->GetFactionId(),
                    std::nullopt,  // previousFactionId - set when base is captured
                    pBase->GetBaseSize()
                });
            }
        }
    }
    m_pWorldDisplay->SetBaseInfo(baseInfo);
    m_pWorldDisplay->SetSelectedUnit(m_pSelectedUnit);
}

bool WorldView::HandleKey(const KeyEvent_t& rEvent)
{
    if (m_pUnitOrderInputController->HandleKey(rEvent, m_pSelectedUnit))
    {
        return true;
    }

    if (m_pCameraInputController->HandleKey(rEvent))
    {
        return true;
    }

    if (rEvent.key == Key_t::Escape)
    {
        m_onRequestExit();
        return true;
    }
    else if (rEvent.key == Key_t::Enter)
    {
        m_onProcessTurn();
        return true;
    }

    return false;
}

void WorldView::HandleMouse(const MouseEvent_t& rEvent)
{
    const WorldMap* pWorldMap = m_rGameState.GetWorldMap();
    if (!pWorldMap)
    {
        return;
    }
    const float tileSize = m_pWorldDisplay->GetEffectiveTileSize();
    const int camX = m_pWorldDisplay->GetCameraX();
    const int camY = m_pWorldDisplay->GetCameraY();

    auto tile = TileHitTester::HitTestWorldGrid(
        static_cast<float>(rEvent.x), static_cast<float>(rEvent.y),
        m_mapLayout.x, m_mapLayout.y, tileSize,
        m_pWorldDisplay->GetVisibleCols(),
        m_pWorldDisplay->GetVisibleRows());

    const int worldX = tile ? tile->first + camX : -1;
    const int worldY = tile ? tile->second + camY : -1;
    const Tile* pClickedTile = pWorldMap->GetTile(worldX, worldY);

    if (m_pUnitOrderInputController->HandleMouse(rEvent, m_pSelectedUnit, pClickedTile))
    {
        return;
    }

    if (m_pCameraInputController->HandleMouse(rEvent))
    {
        return;
    }

    if (rEvent.button == MouseButton_t::Left)
    {
        if (!rEvent.bPressed || !tile)
        {
            return;
        }

        SelectUnitAtTile_(worldX, worldY);

        if (!m_pSelectedUnit)
        {
            BaseManager* pBase = FindBaseAtTile_(worldX, worldY);
            if (pBase)
            {
                m_onOpenBase(*pBase);
            }
        }
    }
}

BaseManager* WorldView::FindBaseAtTile_(int tileX, int tileY) const
{
    for (const auto& pFaction : m_rGameState.GetFactions())
    {
        for (const auto& pBase : pFaction->GetBases())
        {
            if (pBase && pBase->GetX() == tileX && pBase->GetY() == tileY)
            {
                return pBase.get();
            }
        }
    }
    return nullptr;
}

void WorldView::SelectUnitAtTile_(int tileX, int tileY)
{
    const WorldMap* pWorldMap = m_rGameState.GetWorldMap();
    if (!pWorldMap)
    {
        m_pSelectedUnit = nullptr;
        return;
    }

    const Tile* pTile = pWorldMap->GetTile(tileX, tileY);
    if (!pTile)
    {
        m_pSelectedUnit = nullptr;
        return;
    }

    const std::vector<Unit*>& units = pWorldMap->GetUnitsOnTile(*pTile);
    if (units.empty())
    {
        m_pSelectedUnit = nullptr;
        return;
    }

    m_pSelectedUnit = units[0];
}

} // namespace ac
