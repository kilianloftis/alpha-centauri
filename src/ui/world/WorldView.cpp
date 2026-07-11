#include "ui/world/WorldView.h"
#include <algorithm>
#include "ui/world/InfoPanelElement.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/FactionExploredMap.h"
#include "game/faction/FactionVisibleMap.h"
#include "game/faction/UnitVisibility.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/UnitManager.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"
#include "ui/TileHitTester.h"
#include "graphics/Graphics.h"
#include <string>
#include <memory>

namespace ac
{

namespace
{

constexpr RatioLayout_t k_MapLayout      {0.0f, 0.0f, 1.0f, 0.867f};
constexpr RatioLayout_t k_InfoPanelLayout{0.0f, 0.867f, 1.0f, 0.133f};
constexpr Color_t k_ResearchTextColor      {100, 200, 255, 255};
constexpr size_t k_InfoPanelElementIndex = 0;
constexpr int    k_InvalidTileCoord      = -1;

} // namespace

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
, m_pWorldDisplay(std::make_unique<WorldDisplay>(rGameState, m_mapLayout))
, m_onProcessTurn(std::move(onProcessTurn))
, m_onRequestExit(std::move(onRequestExit))
, m_onOpenBase(std::move(onOpenBase))
, m_pCameraInputController(std::make_unique<CameraInputController>(*m_pWorldDisplay, rWorldMap, m_mapLayout))
, m_pUnitOrderInputController(std::make_unique<UnitOrderInputController>())
{
    m_elements.push_back(std::make_unique<InfoPanelElement>(ResolveLayout(m_layout, k_InfoPanelLayout)));

    // Every faction exists by the time WorldView is constructed (Engine::Initialize_ creates
    // them all up front) and none are added later, so wiring here covers unit death for the
    // whole game: without this, m_pSelectedUnit would dangle the moment a unit dies.
    for (Faction& rFaction : m_rGameState.Factions())
    {
        rFaction.GetUnitManager().OnUnitDestroyed.Connect([this](Unit& rDestroyed) {
            if (m_pSelectedUnit == &rDestroyed)
            {
                m_pSelectedUnit = nullptr;
            }
        });
    }
}

void WorldView::Render(Graphics& rGraphics)
{
    Update_();
    m_pWorldDisplay->Render(rGraphics);
    IGameView::Render(rGraphics);
}

void WorldView::UpdateCameraInput(bool bEnabled)
{
    m_pCameraInputController->Update(bEnabled);
}

void WorldView::Update_()
{
    std::vector<InfoPanelElement::InfoLine> infoLines;
    infoLines.push_back({"Mission Year: " + std::to_string(m_rGameState.GetMissionYear()), Color_t::White()});
    if (const Faction* pPlayerFaction = m_rGameState.GetPlayerFaction())
    {
        infoLines.push_back({"Energy: " + std::to_string(pPlayerFaction->GetEconomy().GetEnergy()), Color_t::Yellow()});
        infoLines.push_back({"Research: " + std::to_string(pPlayerFaction->GetResearch().GetAccumulatedPoints()), k_ResearchTextColor});
    }
    static_cast<InfoPanelElement*>(m_elements[k_InfoPanelElementIndex].get())->SetInfoLines(infoLines);

    m_pWorldDisplay->SetSelectedUnit(m_pSelectedUnit);
}

bool WorldView::HandleKey(const KeyEvent_t& rEvent)
{
    if (m_pUnitOrderInputController->HandleKey(rEvent, GetControllableSelectedUnit_()))
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
    const float tileSize = m_pWorldDisplay->GetEffectiveTileSize();
    const int camX = m_pWorldDisplay->GetCameraX();
    const int camY = m_pWorldDisplay->GetCameraY();

    auto tile = TileHitTester::HitTestWorldGrid(
        static_cast<float>(rEvent.x), static_cast<float>(rEvent.y),
        m_mapLayout.x, m_mapLayout.y, tileSize,
        m_pWorldDisplay->GetVisibleCols(),
        m_pWorldDisplay->GetVisibleRows());

    const int worldX = tile ? tile->first + camX : k_InvalidTileCoord;
    const int worldY = tile ? tile->second + camY : k_InvalidTileCoord;
    const Tile* pClickedTile = m_rGameState.GetWorldMap().GetTile(worldX, worldY);

    if (m_pUnitOrderInputController->HandleMouse(rEvent, GetControllableSelectedUnit_(), pClickedTile))
    {
        return;
    }

    if (rEvent.button == MouseButton_t::Left && rEvent.bPressed && tile)
    {
        const Faction* pPlayer = m_rGameState.GetPlayerFaction();
        const FactionExploredMap* pExplored =
            (pPlayer && pPlayer->GetExploredMap().IsSized()) ? &pPlayer->GetExploredMap() : nullptr;
        const FactionVisibleMap* pVisible =
            (pPlayer && pPlayer->GetVisibleMap().IsSized()) ? &pPlayer->GetVisibleMap() : nullptr;

        if (pExplored && !pExplored->IsExplored(worldX, worldY))
        {
            return;
        }

        if (BaseManager* pBase = m_rGameState.FindBaseAt(worldX, worldY))
        {
            m_onOpenBase(*pBase);
            return;
        }

        if (!pVisible || pVisible->IsVisible(worldX, worldY))
        {
            SelectUnitAtTile_(worldX, worldY);
        }
    }
}

void WorldView::SelectUnitAtTile_(int tileX, int tileY)
{
    const WorldMap& rWorldMap = m_rGameState.GetWorldMap();
    const Tile* pTile = rWorldMap.GetTile(tileX, tileY);
    if (!pTile)
    {
        m_pSelectedUnit = nullptr;
        return;
    }

    const std::vector<Unit*>& units = rWorldMap.GetUnitsOnTile(*pTile);
    if (units.empty())
    {
        m_pSelectedUnit = nullptr;
        return;
    }

    const Faction* pPlayer = m_rGameState.GetPlayerFaction();
    for (Unit* pUnit : units)
    {
        if (!pUnit)
        {
            continue;
        }
        if (pPlayer && !IsUnitVisibleTo(*pPlayer, *pUnit, m_rGameState.GetTileEffects()))
        {
            continue;
        }
        m_pSelectedUnit = pUnit;
        return;
    }

    m_pSelectedUnit = nullptr;
}

Unit* WorldView::GetControllableSelectedUnit_() const
{
    if (m_pSelectedUnit && m_pSelectedUnit->GetFaction().IsPlayerControlled())
    {
        return m_pSelectedUnit;
    }
    return nullptr;
}

} // namespace ac
