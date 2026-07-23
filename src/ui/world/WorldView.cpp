#include "ui/world/WorldView.h"
#include <algorithm>
#include "ui/world/InfoPanelElement.h"
#include "ui/world/LocationPanel.h"
#include "ui/world/SelectedUnitPanel.h"
#include "ui/world/SupplyCrawlPopup.h"
#include "ui/world/UnitStackPanel.h"
#include "ui/world/CommlinksButton.h"
#include "ui/world/EndTurnButton.h"
#include "game/GameState.h"
#include "game/GameSettings.h"
#include "game/Faction.h"
#include "game/faction/FactionExploredMap.h"
#include "game/faction/UnitVisibility.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/UnitManager.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/effects/EffectEnums.h"
#include "ui/TileHitTester.h"
#include "ui/style/UiStyle.h"
#include "graphics/Graphics.h"
#include <string>
#include <memory>

namespace ac
{

namespace
{

constexpr int    k_InvalidTileCoord  = -1;

} // namespace

WorldView::WorldView(
    GameState& rGameState,
    const WorldMap& rWorldMap,
    WindowLayout_t layout,
    std::function<void()> onProcessTurn,
    std::function<void()> onRequestExit,
    OpenBaseCallback_t onOpenBase,
    OpenCombatCallback_t onOpenCombat,
    std::function<void()> onOpenCommlinks
)
: IGameView(layout)
, m_rGameState(rGameState)
, m_mapLayout(ResolveLayout(layout, Style().layouts.map))
, m_pWorldDisplay(std::make_unique<WorldDisplay>(rGameState, m_mapLayout))
, m_onProcessTurn(std::move(onProcessTurn))
, m_onRequestExit(std::move(onRequestExit))
, m_onOpenBase(std::move(onOpenBase))
, m_onOpenCombat(std::move(onOpenCombat))
, m_onOpenCommlinks(std::move(onOpenCommlinks))
, m_pCameraInputController(std::make_unique<CameraInputController>(*m_pWorldDisplay, rWorldMap, m_mapLayout))
, m_pUnitOrderInputController(std::make_unique<UnitOrderInputController>())
{
    auto pSelectedUnit = std::make_unique<SelectedUnitPanel>(ResolveLayout(m_layout, Style().layouts.leftPanel));
    m_pSelectedUnitPanel = pSelectedUnit.get();
    m_elements.push_back(std::move(pSelectedUnit));

    auto pLocation = std::make_unique<LocationPanel>(ResolveLayout(m_layout, Style().layouts.locationPanel));
    m_pLocationPanel = pLocation.get();
    m_elements.push_back(std::move(pLocation));

    auto pInfo = std::make_unique<InfoPanelElement>(ResolveLayout(m_layout, Style().layouts.centerPanel));
    m_pInfoPanel = pInfo.get();
    m_elements.push_back(std::move(pInfo));

    auto pUnitStack = std::make_unique<UnitStackPanel>(
        ResolveLayout(m_layout, Style().layouts.bottomPanel),
        [this](Unit& rUnit) { SetSelectedUnit_(&rUnit, true); });
    m_pUnitStackPanel = pUnitStack.get();
    m_elements.push_back(std::move(pUnitStack));

    m_elements.push_back(std::make_unique<CommlinksButton>(
        ResolveLayout(m_layout, Style().layouts.rightButton),
        [this]() { m_onOpenCommlinks(); }));

    auto pEndTurn = std::make_unique<EndTurnButton>(
        ResolveLayout(Style().layouts.rightPanel, Style().endTurnButton.layout),
        [this]() { m_onProcessTurn(); });
    m_pEndTurnButton = pEndTurn.get();
    m_elements.push_back(std::move(pEndTurn));

    // Every faction exists by the time WorldView is constructed (Engine::Initialize_ creates
    // them all up front) and none are added later, so wiring here covers unit death for the
    // whole game: without this, m_pSelectedUnit would dangle the moment a unit dies.
    for (Faction& rFaction : m_rGameState.Factions())
    {
        rFaction.GetUnitManager().OnUnitDestroyed.Connect([this](Unit& rDestroyed) {
            if (m_pSelectedUnit == &rDestroyed)
            {
                SetSelectedUnit_(nullptr, false);
            }
        });
    }
}

void WorldView::Render(Graphics& rGraphics)
{
    Update_();
    m_pWorldDisplay->Render(rGraphics);
    if (!m_bSuppressDashboard)
    {
        IGameView::Render(rGraphics);
    }
}

void WorldView::UpdateCameraInput(bool bEnabled)
{
    m_pCameraInputController->Update(bEnabled);
}

void WorldView::SetSuppressDashboard(bool bSuppress)
{
    m_bSuppressDashboard = bSuppress;
}

bool WorldView::PlayerUnitsNeedOrders_() const
{
    const Faction* pPlayer = m_rGameState.GetPlayerFaction();
    if (!pPlayer)
    {
        return false;
    }
    return pPlayer->GetUnitManager().HasUnitsRequiringOrders();
}

bool WorldView::UnitRequiresOrders_(const Unit& rUnit)
{
    return !rUnit.GetOrder().has_value() && rUnit.GetMoveFragmentsRemaining() > 0;
}

void WorldView::SetSelectedUnit_(Unit* pUnit, bool bManualSelection)
{
    const bool bSelectionChanged = m_pSelectedUnit != pUnit;
    m_pSelectedUnit = pUnit;
    m_bManualSelection = bManualSelection;

    // Keep location / stack panels on the selected unit's tile when cycling or picking.
    if (pUnit)
    {
        m_pSelectedTile = &pUnit->GetTile();
    }

    if (bSelectionChanged && pUnit && !bManualSelection)
    {
        const Tile& rTile = pUnit->GetTile();
        m_pCameraInputController->CenterOnTile(rTile.GetX(), rTile.GetY());
    }
}

void WorldView::SetSelectedTile_(const Tile* pTile)
{
    m_pSelectedTile = pTile;
}

void WorldView::SelectNextAvailableUnit_()
{
    const Faction* pPlayer = m_rGameState.GetPlayerFaction();
    SetSelectedUnit_(
        pPlayer ? pPlayer->GetUnitManager().GetNextAvailableUnit() : nullptr,
        false);
}

void WorldView::SelectNextAvailableUnitIfNeeded_()
{
    const Faction* pPlayer = m_rGameState.GetPlayerFaction();
    if (!pPlayer)
    {
        return;
    }

    // Manual map browse: empty tile (no unit) or a unit that does not need orders
    // (already moved / has an order). Leave selection alone so the player can inspect.
    if (m_bManualSelection && (!m_pSelectedUnit || !UnitRequiresOrders_(*m_pSelectedUnit)))
    {
        return;
    }

    if (m_pSelectedUnit && UnitRequiresOrders_(*m_pSelectedUnit))
    {
        return;
    }

    // Empty selection, or the auto-cycled unit is done (out of moves / has an order).
    if (Unit* pNext = pPlayer->GetUnitManager().GetNextAvailableUnit())
    {
        SetSelectedUnit_(pNext, false);
    }
}

void WorldView::Update_()
{
    const auto& s = Style().worldView;
    std::vector<InfoPanelElement::InfoLine> infoLines;
    infoLines.push_back({"Mission Year: " + std::to_string(m_rGameState.GetMissionYear()), s.missionYearColor});
    if (const Faction* pPlayerFaction = m_rGameState.GetPlayerFaction())
    {
        infoLines.push_back({"Energy: " + std::to_string(pPlayerFaction->GetEconomy().GetEnergy()), s.energyTextColor});
        infoLines.push_back({"Research: " + std::to_string(pPlayerFaction->GetResearch().GetAccumulatedPoints()), s.researchTextColor});
    }
    m_pInfoPanel->SetInfoLines(infoLines);

    if (!m_bSuppressDashboard)
    {
        SelectNextAvailableUnitIfNeeded_();
    }

    m_pSelectedUnitPanel->SetSelectedUnit(m_pSelectedUnit);
    m_pLocationPanel->SetSelectedTile(m_pSelectedTile);

    std::vector<Unit*> stackUnits;
    if (m_pSelectedTile)
    {
        const Faction* pPlayer = m_rGameState.GetPlayerFaction();
        for (Unit* pUnit : m_rGameState.GetWorldMap().GetUnitsOnTile(*m_pSelectedTile))
        {
            if (!pUnit)
            {
                continue;
            }
            if (pPlayer && !IsUnitVisibleTo(*pPlayer, *pUnit, m_rGameState.GetTileEffects()))
            {
                continue;
            }
            stackUnits.push_back(pUnit);
        }
    }
    m_pUnitStackPanel->SetUnits(std::move(stackUnits), m_pSelectedUnit);

    const bool bNeedOrders = PlayerUnitsNeedOrders_();
    const bool bPauseAtEnd = m_rGameState.GetSettings().IsPauseAtEndOfTurn();

    // Ready highlight only when pause-at-end is on and the interaction phase is finished.
    m_pEndTurnButton->SetReady(bPauseAtEnd && !bNeedOrders && !m_bSuppressDashboard);

    // When pause is off, advance automatically once the last unit that needed orders is done.
    // Skip while CombatView is open (ProcessTurn_ also rejects overlays).
    if (!bPauseAtEnd && !m_bSuppressDashboard && m_bHadUnitsNeedingOrders && !bNeedOrders)
    {
        m_bHadUnitsNeedingOrders = false;
        m_onProcessTurn();
    }
    else
    {
        m_bHadUnitsNeedingOrders = bNeedOrders;
    }

    m_pWorldDisplay->SetSelectedUnit(m_pSelectedUnit);
    m_pWorldDisplay->SetPathPreview(m_pUnitOrderInputController->GetPathPreview());
}

bool WorldView::HandleKey(const KeyEvent_t& rEvent)
{
    // Popups / chrome first (Escape to dismiss SupplyCrawlPopup, etc.).
    for (int i = static_cast<int>(m_elements.size()) - 1; i >= 0; --i)
    {
        if (m_elements[static_cast<size_t>(i)]->HandleKey(rEvent))
        {
            return true;
        }
    }

    Unit* pControllable = GetControllableSelectedUnit_();
    if (m_pUnitOrderInputController->HandleKey(rEvent, pControllable))
    {
        if (m_pUnitOrderInputController->WasSupplyCrawlRequested() && pControllable)
        {
            Unit* pUnit = pControllable;
            m_elements.push_back(std::make_unique<SupplyCrawlPopup>(
                ResolveLayout(m_layout, Style().layouts.popupSmall),
                [this, pUnit](StatId_t resource) {
                    if (m_pSelectedUnit != pUnit)
                    {
                        return;
                    }
                    if (pUnit->TryStartSupplyCrawl(resource))
                    {
                        SelectNextAvailableUnit_();
                    }
                }));
            return true;
        }
        if (m_pUnitOrderInputController->WasOrderAssigned())
        {
            SelectNextAvailableUnit_();
        }
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
    else if (rEvent.key == Key_t::V)
    {
        const Faction* pPlayer = m_rGameState.GetPlayerFaction();
        SetSelectedUnit_(
            pPlayer ? pPlayer->GetUnitManager().GetNextAvailableUnit(m_pSelectedUnit) : nullptr,
            false);
        return true;
    }

    return false;
}

void WorldView::HandleMouse(const MouseEvent_t& rEvent)
{
    // UI chrome (End Turn) above the map takes priority over tile/unit input.
    if (rEvent.bPressed)
    {
        const float x = static_cast<float>(rEvent.x);
        const float y = static_cast<float>(rEvent.y);
        for (int i = static_cast<int>(m_elements.size()) - 1; i >= 0; --i)
        {
            if (m_elements[static_cast<size_t>(i)]->Contains(x, y))
            {
                m_elements[static_cast<size_t>(i)]->HandleMouseClick(rEvent);
                return;
            }
        }
    }

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

    Unit* pControllable = GetControllableSelectedUnit_();
    const bool bOrderHandled = m_pUnitOrderInputController->HandleMouse(
        rEvent, pControllable, pClickedTile, &m_rGameState.GetPathfinder());

    // Left-click release on an explored tile selects the tile and a visible unit on it
    // (also when a move / attack order consumed the click — tile only in that case until
    // the normal pick path below runs).
    if (rEvent.button == MouseButton_t::Left && !rEvent.bPressed && pClickedTile)
    {
        const Faction* pPlayer = m_rGameState.GetPlayerFaction();
        const FactionExploredMap* pExplored =
            (pPlayer && pPlayer->GetExploredMap().IsSized()) ? &pPlayer->GetExploredMap() : nullptr;
        if (!pExplored || pExplored->IsExplored(worldX, worldY))
        {
            SetSelectedTile_(pClickedTile);
        }
    }

    if (bOrderHandled)
    {
        if (m_pUnitOrderInputController->WasAttackRequested() && pControllable
            && m_pUnitOrderInputController->GetAttackTarget())
        {
            TryBeginAttack_(*pControllable, *m_pUnitOrderInputController->GetAttackTarget());
            return;
        }
        if (m_pUnitOrderInputController->WasOrderAssigned() && pControllable)
        {
            m_rGameState.GetUnitOrderExecutor().Execute(*pControllable);
            SelectNextAvailableUnit_();
        }
        return;
    }

    if (rEvent.button == MouseButton_t::Left && !rEvent.bPressed && tile)
    {
        const Faction* pPlayer = m_rGameState.GetPlayerFaction();
        const FactionExploredMap* pExplored =
            (pPlayer && pPlayer->GetExploredMap().IsSized()) ? &pPlayer->GetExploredMap() : nullptr;

        if (pExplored && !pExplored->IsExplored(worldX, worldY))
        {
            return;
        }

        // Unit pick uses IsUnitVisibleTo (fog / Conceal / contact reveal), not tile fog alone.
        SelectUnitAtTile_(worldX, worldY);

        if (BaseManager* pBase = m_rGameState.FindBaseAt(worldX, worldY))
        {
            m_onOpenBase(*pBase);
            return;
        }
    }
}

void WorldView::TryBeginAttack_(Unit& rAttacker, const Tile& rTargetTile)
{
    // Capture identity / tiles before Resolve mutates positions / destroys units.
    const Tile& rAttackerTile = rAttacker.GetTile();
    const std::string attackerName = rAttacker.GetDesign().GetName();
    const std::string defenderName = FindUnitNameOnTile_(rTargetTile);

    const auto result = m_rGameState.GetUnitOrderExecutor().TryAttack(rAttacker, rTargetTile);
    if (!result)
    {
        return;
    }

    if (result->rounds.empty())
    {
        SelectNextAvailableUnit_();
        return;
    }

    SetSuppressDashboard(true);
    m_onOpenCombat(
        *result,
        rAttackerTile,
        rTargetTile,
        attackerName,
        defenderName,
        *m_pWorldDisplay,
        m_mapLayout,
        [this]() {
            SetSuppressDashboard(false);
            SelectNextAvailableUnit_();
        });
}

std::string WorldView::FindUnitNameOnTile_(const Tile& rTile) const
{
    const std::vector<Unit*>& units = m_rGameState.GetWorldMap().GetUnitsOnTile(rTile);
    for (const Unit* pUnit : units)
    {
        if (pUnit)
        {
            return pUnit->GetDesign().GetName();
        }
    }
    return {};
}

void WorldView::SelectUnitAtTile_(int tileX, int tileY)
{
    const WorldMap& rWorldMap = m_rGameState.GetWorldMap();
    const Tile* pTile = rWorldMap.GetTile(tileX, tileY);
    if (!pTile)
    {
        SetSelectedUnit_(nullptr, true);
        return;
    }

    const std::vector<Unit*>& units = rWorldMap.GetUnitsOnTile(*pTile);
    if (units.empty())
    {
        SetSelectedUnit_(nullptr, true);
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
        SetSelectedUnit_(pUnit, true);
        return;
    }

    SetSelectedUnit_(nullptr, true);
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
