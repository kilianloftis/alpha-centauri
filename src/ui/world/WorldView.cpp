#include "ui/world/WorldView.h"
#include <algorithm>
#include "ui/world/InfoPanelElement.h"
#include "ui/world/LocationPanel.h"
#include "ui/world/SelectedUnitPanel.h"
#include "ui/world/SupplyCrawlPopup.h"
#include "ui/world/ProbeActionPopup.h"
#include "ui/world/UnitStackPanel.h"
#include "ui/world/CommlinksButton.h"
#include "ui/world/EndTurnButton.h"
#include "ui/world/MinimapDisplay.h"
#include "game/GameState.h"
#include "game/GameDataContext.h"
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
#include "game/units/UnitOrderExecutor.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitOrder.h"
#include "game/units/ProbeActionResult.h"
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
    GameDataContext& rGameDataContext,
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
, m_rGameDataContext(rGameDataContext)
, m_mapLayout(ResolveLayout(layout, Style().layouts.map))
, m_pWorldDisplay(std::make_unique<WorldDisplay>(rGameState, m_mapLayout))
, m_onProcessTurn(std::move(onProcessTurn))
, m_onRequestExit(std::move(onRequestExit))
, m_onOpenBase(std::move(onOpenBase))
, m_onOpenCombat(std::move(onOpenCombat))
, m_onOpenCommlinks(std::move(onOpenCommlinks))
, m_pCameraInputController(std::make_unique<CameraInputController>(*m_pWorldDisplay, rWorldMap, m_mapLayout))
, m_pUnitOrderInputController(std::make_unique<UnitOrderInputController>())
, m_pTerraformInputController(std::make_unique<TerraformInputController>())
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

    const WindowLayout_t rightPanel = ResolveLayout(m_layout, Style().layouts.rightPanel);
    m_elements.push_back(std::make_unique<MinimapDisplay>(
        m_rGameState,
        rightPanel,
        m_pWorldDisplay->GetViewport(),
        [this](int tileX, int tileY) {
            m_pCameraInputController->CenterOnTile(tileX, tileY);
        }));

    // Nested in rightPanel so End Turn sits on top of the minimap (and receives clicks first).
    auto pEndTurn = std::make_unique<EndTurnButton>(
        ResolveLayout(rightPanel, Style().endTurnButton.layout),
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
        // Transfer preserves identity (the pointer does not dangle), but a unit that just
        // left this faction is no longer "my" selection — drop it rather than keep showing
        // a foreign unit as selected (see docs/architecture/high-level.md, "Object lifetime").
        rFaction.GetUnitManager().OnUnitReleased.Connect([this](Unit& rReleased) {
            if (m_pSelectedUnit == &rReleased)
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

void WorldView::ProcessPendingAutoEndTurn()
{
    if (!m_bPendingAutoEndTurn)
    {
        return;
    }
    // Keep the request queued while an in-view modal blocks Advance; soft-gate in
    // Engine::ProcessTurn_ would otherwise drop it after clearing the flag.
    if (BlocksTurnAdvance())
    {
        return;
    }
    m_bPendingAutoEndTurn = false;
    m_onProcessTurn();
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
    // Short moves clear the order but leave move fragments — stay on this unit so the
    // player can keep issuing orders. GetNextAvailableUnit() alone would jump to the
    // first faction unit that needs orders, which is often a different unit.
    if (m_pSelectedUnit && UnitRequiresOrders_(*m_pSelectedUnit))
    {
        SetSelectedUnit_(m_pSelectedUnit, m_bManualSelection);
        return;
    }

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
    // Queue even if an in-view modal currently blocks Advance — ProcessPendingAutoEndTurn
    // keeps the flag until CanAdvanceTurn is clear. Do not call m_onProcessTurn() here:
    // Update_() runs from Render(), and Advance must never run on the paint path.
    if (!bPauseAtEnd && !m_bSuppressDashboard && m_bHadUnitsNeedingOrders && !bNeedOrders)
    {
        m_bHadUnitsNeedingOrders = false;
        m_bPendingAutoEndTurn = true;
    }
    else if (!m_bPendingAutoEndTurn)
    {
        m_bHadUnitsNeedingOrders = bNeedOrders;
    }

    m_pWorldDisplay->SetSelectedUnit(m_pSelectedUnit);
    m_pWorldDisplay->SetPathPreview(m_pUnitOrderInputController->GetPathPreview());
}

bool WorldView::HandleKey(const KeyEvent_t& rEvent)
{
    // Exclusive modal capture (probe/supply popups, ...): while one is open it is the only
    // thing that sees the key, so Enter cannot end the turn and map/unit hotkeys cannot
    // mutate selection/orders underneath it. Always return true so UIManager does not run
    // global view shortcuts (F2/E/…) while a Unit*-capturing modal is open on WorldView.
    if (UIElement* pModal = GetTopModalElement())
    {
        (void)pModal->HandleKey(rEvent);
        return true;
    }

    Unit* pControllable = GetControllableSelectedUnit_();
    if (m_pUnitOrderInputController->HandleKey(rEvent, pControllable))
    {
        if (m_pUnitOrderInputController->WasAttachTransportRequested())
        {
            if (pControllable
                && m_rGameState.GetUnitOrderExecutor().TryAttachToTransport(*pControllable))
            {
                return true;
            }
            // Attach failed — fall through so terraform can use L (LevelTerrain).
        }
        else if (m_pUnitOrderInputController->WasUnloadTransportRequested())
        {
            if (pControllable)
            {
                m_rGameState.GetUnitOrderExecutor().TryUnloadTransport(*pControllable);
            }
            return true;
        }
        else if (m_pUnitOrderInputController->WasSupplyCrawlRequested() && pControllable)
        {
            Unit* pUnit = pControllable;
            DismissOpenModals_();
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
        else if (m_pUnitOrderInputController->WasFoundBaseRequested() && pControllable)
        {
            if (m_rGameState.GetUnitOrderExecutor().TryFoundBase(
                    *pControllable, m_rGameState, m_rGameDataContext))
            {
                // DestroyUnit clears selection via OnUnitDestroyed; pick the next unit.
                SelectNextAvailableUnit_();
            }
            return true;
        }
        else if (m_pUnitOrderInputController->WasProbeActionRequested() && pControllable
                 && m_pUnitOrderInputController->GetProbeTarget())
        {
            TryOpenProbeActions_(*pControllable, *m_pUnitOrderInputController->GetProbeTarget());
            return true;
        }
        else
        {
            if (m_pUnitOrderInputController->WasOrderAssigned())
            {
                SelectNextAvailableUnit_();
            }
            return true;
        }
    }

    if (m_pTerraformInputController->HandleKey(rEvent, pControllable))
    {
        if (m_pTerraformInputController->WasTerraformRequested() && pControllable)
        {
            if (m_rGameState.GetUnitOrderExecutor().TryStartTerraform(
                    *pControllable, m_pTerraformInputController->GetRequestedImprovementId(),
                    m_rGameState))
            {
                SelectNextAvailableUnit_();
            }
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
    // Exclusive modal capture: every press goes to the topmost modal element, even outside
    // its own Contains rect (so outside-click dismiss works), and neither chrome nor the map
    // underneath ever sees it. Release events are dropped, matching IGameView's default.
    if (UIElement* pModal = GetTopModalElement())
    {
        if (rEvent.bPressed)
        {
            pModal->HandleMouseClick(rEvent);
        }
        return;
    }

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

    const MapViewport& rViewport = m_pWorldDisplay->GetViewport();

    auto tile = TileHitTester::HitTestWorldGrid(
        static_cast<float>(rEvent.x), static_cast<float>(rEvent.y),
        m_mapLayout.x, m_mapLayout.y, rViewport.TileSize(),
        rViewport.VisibleCols(),
        rViewport.VisibleRows());

    const auto worldCoords = tile
        ? rViewport.WorldCoordsAt(tile->first, tile->second)
        : std::nullopt;
    const int worldX = worldCoords ? worldCoords->first : k_InvalidTileCoord;
    const int worldY = worldCoords ? worldCoords->second : k_InvalidTileCoord;
    const Tile* pClickedTile = m_rGameState.GetWorldMap().GetTile(worldX, worldY);

    Unit* pControllable = GetControllableSelectedUnit_();
    const bool bOrderHandled = m_pUnitOrderInputController->HandleMouse(
        rEvent, pControllable, pClickedTile, &m_rGameState.GetPathfinder(), &m_rGameState,
        &m_rGameDataContext);

    if (bOrderHandled)
    {
        const bool bDidOrderAction =
            m_pUnitOrderInputController->WasAttackRequested()
            || m_pUnitOrderInputController->WasProbeActionRequested()
            || m_pUnitOrderInputController->WasOrderAssigned();

        // Move / attack / probe still focus the location panel on the target tile.
        // An aborted long-press (held past threshold, no order) must not change selection.
        if (bDidOrderAction && rEvent.button == MouseButton_t::Left && !rEvent.bPressed
            && pClickedTile)
        {
            const Faction* pPlayer = m_rGameState.GetPlayerFaction();
            const FactionExploredMap* pExplored =
                (pPlayer && pPlayer->GetExploredMap().IsSized()) ? &pPlayer->GetExploredMap()
                                                                : nullptr;
            if (!pExplored || pExplored->IsExplored(worldX, worldY))
            {
                SetSelectedTile_(pClickedTile);
            }
        }

        if (m_pUnitOrderInputController->WasAttackRequested() && pControllable
            && m_pUnitOrderInputController->GetAttackTarget())
        {
            TryBeginAttack_(*pControllable, *m_pUnitOrderInputController->GetAttackTarget());
            return;
        }
        if (m_pUnitOrderInputController->WasProbeActionRequested() && pControllable
            && m_pUnitOrderInputController->GetProbeTarget())
        {
            TryOpenProbeActions_(*pControllable, *m_pUnitOrderInputController->GetProbeTarget());
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

        SetSelectedTile_(pClickedTile);

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

void WorldView::TryOpenProbeActions_(Unit& rProbe, const Tile& rTargetTile)
{
    std::vector<std::pair<ProbeActionId_t, std::string>> actions =
        m_rGameState.GetProbeActions().ListAvailableProbeActions(
            rProbe, rTargetTile, m_rGameState, m_rGameDataContext);
    if (actions.empty())
    {
        return;
    }

    // Capture the tile, not the base/unit on it: WorldMap owns tiles for the whole game,
    // whereas the target may be captured or killed while the popup is open. TryProbeAction
    // re-resolves and rejects cleanly if it is gone.
    Unit* pProbe = &rProbe;
    const Tile* pTargetTile = &rTargetTile;
    DismissOpenModals_();
    m_elements.push_back(std::make_unique<ProbeActionPopup>(
        ResolveLayout(m_layout, Style().layouts.popupSmall),
        std::move(actions),
        [this, pProbe, pTargetTile](ProbeActionId_t actionId) {
            if (m_pSelectedUnit != pProbe)
            {
                return;
            }
            const ProbeActionResult_t result =
                m_rGameState.GetProbeActions().TryProbeAction(
                    *pProbe, actionId, *pTargetTile, m_rGameState, m_rGameDataContext);
            if (result.outcome != ProbeActionOutcome_t::Rejected)
            {
                SelectNextAvailableUnit_();
            }
        }));
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
