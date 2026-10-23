#include "ui/world/WorldView.h"
#include <algorithm>
#include "ui/world/InfoPanelElement.h"
#include "ui/world/LocationPanel.h"
#include "ui/world/SelectedUnitPanel.h"
#include "ui/ConfirmPopup.h"
#include "ui/ListSelectorPopup.h"
#include "ui/NoticePopup.h"
#include "ui/ScrapRefundText.h"
#include "ui/world/UnitStackPanel.h"
#include "ui/world/CommlinksButton.h"
#include "ui/world/EndTurnButton.h"
#include "ui/world/MinimapDisplay.h"
#include "game/GameState.h"
#include "game/GameDataContext.h"
#include "game/combat/MoraleCalculator.h"
#include "game/GameSettings.h"
#include "game/faction/Faction.h"
#include "game/movement/TransportMutations.h"
#include "game/visibility/FactionExploredMap.h"
#include "game/visibility/VisibilityRules.h"
#include "game/base/BaseManager.h"
#include "game/production/ProductionManager.h"
#include "game/production/IConstructable.h"
#include "game/economy/EconomyManager.h"
#include "game/research/ResearchManager.h"
#include "game/units/UnitManager.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/movement/FuelMutations.h"
#include "game/movement/AirdropRules.h"
#include "game/combat/AttackRules.h"
#include "ui/HotkeyConfig.h"
#include "ui/SpriteLibrary.h"
#include "ui/world/AirdropFailMessages.h"
#include "game/units/UnitOrderExecutor.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitOrder.h"
#include "game/probes/ProbeActionResult.h"
#include "game/effects/EffectEnums.h"
#include "ui/style/UiStyle.h"
#include "graphics/Graphics.h"
#include <magic_enum.hpp>
#include <string>
#include <memory>
#include <optional>
#include <unordered_map>
#include <stdexcept>
#include <vector>

namespace ac
{

namespace
{

constexpr int    k_InvalidTileCoord  = -1;

} // namespace

WorldView::WorldView(
    GameState& rGameState,
    const HotkeyConfig& rHotkeys,
    const WorldMap& rWorldMap,
    MapRenderer& rMapRenderer,
    SpriteLibrary& rSprites,
    WindowLayout_t layout,
    std::function<void()> onProcessTurn,
    std::function<void()> onRequestExit,
    OpenBaseCallback_t onOpenBase,
    OpenCombatCallback_t onOpenCombat,
    std::function<void()> onOpenCommlinks
)
: IWorldView(layout)
, m_rGameState(rGameState)
, m_rHotkeys(rHotkeys)
, m_rSprites(rSprites)
, m_consolePlacement(PlaceFixedLayout(layout, Style().layouts.console))
, m_mapLayout(MapBandAbove(
      layout, m_consolePlacement.layout, m_consolePlacement.mapOverlap))
, m_pWorldDisplay(std::make_unique<WorldDisplay>(rGameState, rMapRenderer, m_mapLayout))
, m_onProcessTurn(std::move(onProcessTurn))
, m_onRequestExit(std::move(onRequestExit))
, m_onOpenBase(std::move(onOpenBase))
, m_onOpenCombat(std::move(onOpenCombat))
, m_onOpenCommlinks(std::move(onOpenCommlinks))
, m_pCameraInputController(std::make_unique<CameraInputController>(
      *m_pWorldDisplay, rWorldMap, m_mapLayout, rHotkeys))
, m_pUnitOrderInputController(std::make_unique<UnitOrderInputController>(rHotkeys, rGameState))
{
    AddConsolePanels_(rMapRenderer);
    AddMinimapAndEndTurn_(rMapRenderer);
    WatchSelectionDepartures_();
}

void WorldView::AddConsolePanels_(MapRenderer& rMapRenderer)
{
    const auto& console = Style().worldView;
    const WindowLayout_t& rConsole = m_consolePlacement.layout;
    const MoraleCalculator* pMorale = m_rGameState.GetGameData().moraleCalculator.get();
    if (!pMorale)
    {
        throw std::runtime_error("WorldView: morale calculator is not loaded");
    }
    m_pSelectedUnitPanel = &Adopt_(std::make_unique<SelectedUnitPanel>(
        ResolveLayout(rConsole, console.consoleUnit), m_rSprites, *pMorale));
    m_pLocationPanel = &Adopt_(std::make_unique<LocationPanel>(
        m_rGameState, rMapRenderer, m_rSprites, ResolveLayout(rConsole, console.consoleLocation)));
    m_pInfoPanel = &Adopt_(std::make_unique<InfoPanelElement>(
        ResolveLayout(rConsole, console.consoleInfo), m_rSprites));
    m_pUnitStackPanel = &Adopt_(std::make_unique<UnitStackPanel>(
        ResolveLayout(rConsole, console.consoleStack), m_rSprites,
        [this](Unit& rUnit) { SetSelectedUnit_(&rUnit, true); }));
    m_elements.push_back(std::make_unique<CommlinksButton>(
        ResolveLayout(rConsole, console.consoleCommlinks),
        [this]() { m_onOpenCommlinks(); }));
}

void WorldView::AddMinimapAndEndTurn_(MapRenderer& rMapRenderer)
{
    const WindowLayout_t rightPanel =
        ResolveLayout(m_consolePlacement.layout, Style().worldView.consoleMinimap);
    m_elements.push_back(std::make_unique<MinimapDisplay>(
        m_rGameState,
        rMapRenderer,
        rightPanel,
        m_pWorldDisplay->GetViewport(),
        [this](int tileX, int tileY) {
            m_pCameraInputController->CenterOnTile(tileX, tileY);
        }));
    // Nested in rightPanel so End Turn sits on top of the minimap (and receives clicks first).
    m_pEndTurnButton = &Adopt_(std::make_unique<EndTurnButton>(
        ResolveLayout(rightPanel, Style().endTurnButton.layout),
        [this]() { m_onProcessTurn(); }));
}

// Every faction exists by now and none are added later, so this covers unit death for the
// whole game. A unit that left this faction is no longer the player's selection, though still
// alive.
void WorldView::WatchSelectionDepartures_()
{
    for (Faction& rFaction : m_rGameState.Factions())
    {
        UnitManager& rUnits = rFaction.GetUnitManager();
        rUnits.OnUnitDestroyed.Connect(std::bind_front(&WorldView::DeselectIfSelected_, this));
        rUnits.OnUnitReleased.Connect(std::bind_front(&WorldView::DeselectIfSelected_, this));
    }
}

void WorldView::DeselectIfSelected_(Unit& rUnit)
{
    if (m_pSelectedUnit == &rUnit)
    {
        SetSelectedUnit_(nullptr, false);
    }
}

void WorldView::Render(Graphics& rGraphics)
{
    SyncTargetingCursor_(rGraphics);
    m_pWorldDisplay->Render(rGraphics);
    if (!m_bSuppressDashboard)
    {
        DrawConsoleChrome_(rGraphics);
        IWorldView::Render(rGraphics);
    }
}

bool WorldView::UpdateCameraInput(bool bEnabled, std::optional<MousePosition_t> mousePosition)
{
    return m_pCameraInputController->Update(bEnabled, mousePosition);
}

void WorldView::UpdatePresentation()
{
    const MapViewport& rViewport = m_pWorldDisplay->GetViewport();
    const int camXBefore = rViewport.CameraX();
    const int camYBefore = rViewport.CameraY();

    Update_();

    const PresentationSnapshot_t snapshot = CapturePresentationSnapshot_();
    const bool bCameraMovedThisUpdate =
        snapshot.cameraX != camXBefore || snapshot.cameraY != camYBefore;
    if (bCameraMovedThisUpdate || snapshot != m_lastPresented)
    {
        m_bPresentationDirty = true;
    }
    m_lastPresented = snapshot;
}

WorldView::PresentationSnapshot_t WorldView::CapturePresentationSnapshot_() const
{
    const MapViewport& rViewport = m_pWorldDisplay->GetViewport();
    const Path_t* pPath = m_pUnitOrderInputController->GetPathPreview();
    const bool bHasPathTiles = pPath && !pPath->tiles.empty();

    PresentationSnapshot_t snapshot{
        .pSelectedUnit = m_pSelectedUnit,
        .pSelectedTile = m_pSelectedTile,
        .bEndTurnReady = m_pEndTurnButton->IsReady(),
        .bHadPathPreview = pPath != nullptr,
        .pathPreviewLength = pPath ? pPath->tiles.size() : 0,
        .pPathPreviewEnd = bHasPathTiles ? pPath->tiles.back() : nullptr,
        .cameraX = rViewport.CameraX(),
        .cameraY = rViewport.CameraY(),
        .missionYear = m_rGameState.GetMissionYear(),
        .appearanceRevision = m_rGameState.GetWorldMap().GetAppearanceRevision(),
    };
    if (const Faction* pPlayer = m_rGameState.GetPlayerFaction())
    {
        snapshot.energy = pPlayer->GetEconomy().GetEnergy();
        snapshot.research = pPlayer->GetResearch().GetAccumulatedPoints();
        if (pPlayer->GetExploredMap().IsSized())
        {
            snapshot.exploredRevision = pPlayer->GetExploredMap().GetRevision();
        }
        if (pPlayer->GetVisibleMap().IsSized())
        {
            snapshot.visibleRevision = pPlayer->GetVisibleMap().GetRevision();
        }
        snapshot.unitRevision = pPlayer->GetUnitManager().GetRevision();
    }
    return snapshot;
}

bool WorldView::ConsumePresentationDirty()
{
    const bool bDirty = m_bPresentationDirty;
    m_bPresentationDirty = false;
    return bDirty;
}

void WorldView::SetSuppressDashboard(bool bSuppress)
{
    if (m_bSuppressDashboard != bSuppress)
    {
        m_bPresentationDirty = true;
    }
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

void WorldView::CenterOnTile(int tileX, int tileY)
{
    m_pCameraInputController->CenterOnTile(tileX, tileY);
}

void WorldView::PushModal(std::unique_ptr<UIElement> pElement)
{
    if (!pElement)
    {
        throw std::invalid_argument("WorldView::PushModal was given no element");
    }
    DismissOpenModals_();
    m_elements.push_back(std::move(pElement));
}

void WorldView::SetSelectedUnit_(Unit* pUnit, bool bManualSelection)
{
    const bool bSelectionChanged = m_pSelectedUnit != pUnit;
    m_pSelectedUnit = pUnit;
    m_bManualSelection = bManualSelection;

    if (bSelectionChanged)
    {
        ClearAirdropTargeting_();
        ClearBombardTargeting_();
    }

    // Keep location / stack panels on the selected unit's tile when cycling or picking.
    if (pUnit)
    {
        m_pSelectedTile = &pUnit->GetTile();
        // Manual pick cancels skip-turn so the player can issue a real order.
        if (bManualSelection && pUnit->GetOrder().has_value()
            && std::holds_alternative<SkipTurnOrder_t>(*pUnit->GetOrder()))
        {
            pUnit->ClearOrder();
        }
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

void WorldView::TryAutoReturnLowFuel_(Unit& rUnit)
{
    if (!m_rGameState.GetSettings().IsAutoReturnLowFuelAir())
    {
        return;
    }
    if (!TryAssignAutoReturnToFuel(rUnit, m_rGameState.GetPathfinder()))
    {
        return;
    }

    const OrderProgress_t progress = m_rGameState.GetUnitOrderExecutor().Execute(rUnit);
    if (progress == OrderProgress_t::UnitDestroyed)
    {
        SetSelectedUnit_(nullptr, false);
    }
    SelectNextAvailableUnit_();
}

void WorldView::SelectNextAvailableUnit_()
{
    // A short move leaves moves, so stay on this unit rather than jump to the next one.
    if (m_pSelectedUnit && m_pSelectedUnit->RequiresOrders())
    {
        SetSelectedUnit_(m_pSelectedUnit, m_bManualSelection);
        return;
    }

    const Faction* pPlayer = m_rGameState.GetPlayerFaction();
    Unit* pNext = pPlayer ? pPlayer->GetUnitManager().GetNextAvailableUnit(nullptr) : nullptr;
    SetSelectedUnit_(pNext, false);
    if (pNext)
    {
        TryAutoReturnLowFuel_(*pNext);
    }
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
    if (m_bManualSelection && (!m_pSelectedUnit || !m_pSelectedUnit->RequiresOrders()))
    {
        return;
    }

    if (m_pSelectedUnit && m_pSelectedUnit->RequiresOrders())
    {
        return;
    }

    // Empty selection, or the auto-cycled unit is done (out of moves / has an order).
    if (Unit* pNext = pPlayer->GetUnitManager().GetNextAvailableUnit(nullptr))
    {
        SetSelectedUnit_(pNext, false);
        TryAutoReturnLowFuel_(*pNext);
    }
}

void WorldView::Update_()
{
    RefreshInfoPanel_();
    if (!m_bSuppressDashboard)
    {
        SelectNextAvailableUnitIfNeeded_();
    }
    m_pSelectedUnitPanel->SetSelectedUnit(m_pSelectedUnit);
    m_pLocationPanel->SetSelectedTile(m_pSelectedTile);
    m_pUnitStackPanel->SetUnits(VisibleStackUnits_(), m_pSelectedUnit);
    RefreshEndTurnState_();
    m_pWorldDisplay->SetSelectedUnit(m_pSelectedUnit);
    m_pWorldDisplay->SetPathPreview(m_pUnitOrderInputController->GetPathPreview());
}

void WorldView::RefreshInfoPanel_()
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
}

// The selected tile's units the player can see.
std::vector<Unit*> WorldView::VisibleStackUnits_() const
{
    std::vector<Unit*> stackUnits;
    if (!m_pSelectedTile)
    {
        return stackUnits;
    }
    const Faction* pPlayer = m_rGameState.GetPlayerFaction();
    for (Unit* pUnit : m_rGameState.GetWorldMap().GetAllUnitsOnTile(*m_pSelectedTile))
    {
        if (pUnit && (!pPlayer || IsUnitVisibleTo(*pPlayer, *pUnit)))
        {
            stackUnits.push_back(pUnit);
        }
    }
    return stackUnits;
}

void WorldView::RefreshEndTurnState_()
{
    const bool bNeedOrders = PlayerUnitsNeedOrders_();
    const bool bPauseAtEnd = m_rGameState.GetSettings().IsPauseAtEndOfTurn();

    // Ready highlight only when pause-at-end is on and the interaction phase is finished.
    m_pEndTurnButton->SetReady(bPauseAtEnd && !bNeedOrders && !m_bSuppressDashboard);

    // With pause off, queue the auto end-turn once the last unit is done; never run Advance from
    // the paint path.
    if (!bPauseAtEnd && !m_bSuppressDashboard && m_bHadUnitsNeedingOrders && !bNeedOrders)
    {
        m_bHadUnitsNeedingOrders = false;
        m_bPendingAutoEndTurn = true;
    }
    else if (!m_bPendingAutoEndTurn)
    {
        m_bHadUnitsNeedingOrders = bNeedOrders;
    }
}

bool WorldView::HandleKey(const KeyEvent_t& rEvent)
{
    // An open modal element is the only thing that sees the key, and the key always reports
    // handled so global shortcuts cannot fire under it.
    if (UIElement* pModal = GetTopModalElement())
    {
        (void)pModal->HandleKey(rEvent);
        return true;
    }
    Unit* pControllable = GetControllableSelectedUnit_();
    if (m_pUnitOrderInputController->HandleKey(rEvent, pControllable)
        && ApplyOrderInput_(pControllable))
    {
        return true;
    }
    if (pControllable && TrySharedChord_(rEvent, *pControllable))
    {
        return true;
    }
    if (m_pCameraInputController->HandleKey(rEvent))
    {
        return true;
    }
    return HandleChromeKey_(rEvent);
}

// Acts on what the order controller just recognised. False only when an attach fails, so the
// same key can still reach the shared chords.
bool WorldView::ApplyOrderInput_(Unit* pControllable)
{
    UnitOrderInputController& rInput = *m_pUnitOrderInputController;
    if (rInput.WasAttachTransportRequested())
    {
        // Attach failed — fall through so terraform can use L (LevelTerrain).
        return pControllable && TryAttachToTransport(*pControllable);
    }
    if (rInput.WasUnloadTransportRequested())
    {
        if (pControllable)
        {
            TryUnloadTransportInPlace(*pControllable);
        }
        return true;
    }
    if (rInput.WasSupplyCrawlRequested() && pControllable)
    {
        OpenSupplyCrawlMenu_(*pControllable);
        return true;
    }
    if (rInput.WasDisbandRequested() && pControllable)
    {
        OpenDisbandMenu_(*pControllable);
        return true;
    }
    if (rInput.WasFoundBaseRequested() && pControllable)
    {
        FoundBaseWith_(*pControllable);
        return true;
    }
    if (rInput.WasDetonateRequested() && pControllable)
    {
        Detonate_(*pControllable);
        return true;
    }
    if (rInput.WasAirdropModeToggleRequested() && pControllable)
    {
        ToggleAirdropTargeting_();
        return true;
    }
    if (rInput.WasProbeActionRequested() && pControllable && rInput.GetProbeTarget())
    {
        TryOpenProbeActions_(*pControllable, *rInput.GetProbeTarget());
        return true;
    }
    if (rInput.WasOrderAssigned())
    {
        OnOrderAssigned_(pControllable);
    }
    return true;
}

void WorldView::OpenSupplyCrawlMenu_(Unit& rUnit)
{
    Unit* pUnit = &rUnit;
    std::vector<PopupChoice_t> choices;
    for (const StatId_t resource : k_CrawlResources)
    {
        choices.push_back({std::string(magic_enum::enum_name(resource)),
                           [this, pUnit, resource]
                           {
                               if (m_pSelectedUnit != pUnit)
                               {
                                   return;
                               }
                               if (pUnit->TryStartSupplyCrawl(resource))
                               {
                                   SelectNextAvailableUnit_();
                               }
                           }});
    }
    DismissOpenModals_();
    auto pPopup = std::make_unique<ListSelectorPopup>(
        "Supply Crawl", "No resources available", std::move(choices),
        GetPopupLayout(),
        Style().listSelectorPopup, &m_rSprites);
    m_elements.push_back(std::move(pPopup));
}

void WorldView::FoundBaseWith_(Unit& rUnit)
{
    if (m_rGameState.GetUnitOrderExecutor().TryFoundBase(rUnit, m_rGameState))
    {
        // DestroyUnit clears selection via OnUnitDestroyed; pick the next unit.
        SelectNextAvailableUnit_();
    }
}

void WorldView::Detonate_(Unit& rUnit)
{
    if (ApplyDetonation(m_rGameState, rUnit))
    {
        // A DestroyUnit entry in the list clears selection via OnUnitDestroyed.
        SelectNextAvailableUnit_();
    }
}

void WorldView::ToggleAirdropTargeting_()
{
    if (m_bAirdropTargeting)
    {
        ClearAirdropTargeting_();
        return;
    }
    ClearBombardTargeting_();
    m_bAirdropTargeting = true;
    m_bPresentationDirty = true;
}

void WorldView::OnOrderAssigned_(Unit* pControllable)
{
    if (pControllable && pControllable->GetOrder()
        && std::holds_alternative<HoldOrder_t>(*pControllable->GetOrder()))
    {
        m_rGameState.ConsiderHoldLink(*pControllable);
    }
    SelectNextAvailableUnit_();
}

bool WorldView::HandleChromeKey_(const KeyEvent_t& rEvent)
{
    if (m_rHotkeys.Matches(HotkeyAction_t::Cancel, rEvent))
    {
        CancelTargetingOrExit_();
        return true;
    }
    if (m_rHotkeys.Matches(HotkeyAction_t::EndTurn, rEvent))
    {
        m_onProcessTurn();
        return true;
    }
    if (m_rHotkeys.Matches(HotkeyAction_t::NextUnit, rEvent))
    {
        const Faction* pPlayer = m_rGameState.GetPlayerFaction();
        SetSelectedUnit_(
            pPlayer ? pPlayer->GetUnitManager().GetNextAvailableUnit(m_pSelectedUnit) : nullptr,
            false);
        return true;
    }
    return false;
}

void WorldView::CancelTargetingOrExit_()
{
    if (m_bAirdropTargeting || m_bBombardTargeting)
    {
        ClearAirdropTargeting_();
        ClearBombardTargeting_();
        m_bPresentationDirty = true;
        return;
    }
    m_onRequestExit();
}

void WorldView::HandleMouse(const MouseEvent_t& rEvent)
{
    // Every press goes to the topmost modal element, even outside it (outside-click dismiss);
    // release events are dropped.
    if (UIElement* pModal = GetTopModalElement())
    {
        if (rEvent.bPressed)
        {
            pModal->HandleMouseClick(rEvent);
        }
        return;
    }
    // UI chrome (End Turn) above the map takes priority over tile/unit input.
    if (rEvent.bPressed && RouteClickToChrome_(rEvent))
    {
        return;
    }

    const MapViewport& rViewport = m_pWorldDisplay->GetViewport();
    const auto worldCoords = rViewport.WorldCoordsAtPixel(
        static_cast<float>(rEvent.x), static_cast<float>(rEvent.y));
    const int worldX = worldCoords ? worldCoords->first : k_InvalidTileCoord;
    const int worldY = worldCoords ? worldCoords->second : k_InvalidTileCoord;
    const Tile* pClickedTile = m_rGameState.GetWorldMap().GetTile(worldX, worldY);

    Unit* pControllable = GetControllableSelectedUnit_();
    if (TryTargetingClick_(rEvent, pControllable, pClickedTile))
    {
        return;
    }
    if (m_pUnitOrderInputController->HandleMouse(rEvent, pControllable, pClickedTile))
    {
        ApplyOrderClick_(rEvent, pControllable, pClickedTile, worldX, worldY);
        return;
    }
    if (rEvent.button == MouseButton_t::Left && !rEvent.bPressed && worldCoords)
    {
        SelectAtClick_(pClickedTile, worldX, worldY);
    }
}

bool WorldView::RouteClickToChrome_(const MouseEvent_t& rEvent)
{
    const float x = static_cast<float>(rEvent.x);
    const float y = static_cast<float>(rEvent.y);
    for (int i = static_cast<int>(m_elements.size()) - 1; i >= 0; --i)
    {
        if (m_elements[static_cast<size_t>(i)]->Contains(x, y))
        {
            m_elements[static_cast<size_t>(i)]->HandleMouseClick(rEvent);
            return true;
        }
    }
    return false;
}

// A left press while airdrop or bombard targeting is armed picks the target tile.
bool WorldView::TryTargetingClick_(const MouseEvent_t& rEvent, Unit* pControllable,
                                   const Tile* pClickedTile)
{
    if (!pControllable || !pClickedTile || rEvent.button != MouseButton_t::Left
        || !rEvent.bPressed)
    {
        return false;
    }
    if (m_bAirdropTargeting)
    {
        TryCommitAirdrop_(*pControllable, *pClickedTile);
        return true;
    }
    if (m_bBombardTargeting)
    {
        if (TryBeginBombard_(*pControllable, *pClickedTile))
        {
            ClearBombardTargeting_();
            m_bPresentationDirty = true;
        }
        return true;
    }
    return false;
}

void WorldView::ApplyOrderClick_(const MouseEvent_t& rEvent, Unit* pControllable,
                                 const Tile* pClickedTile, int worldX, int worldY)
{
    UnitOrderInputController& rInput = *m_pUnitOrderInputController;
    const bool bDidOrderAction = rInput.WasAttackRequested() || rInput.WasBombardRequested()
                                 || rInput.WasProbeActionRequested() || rInput.WasOrderAssigned();
    // Move / attack / bombard / probe still focus the location panel on the target tile.
    // An aborted long-press (held past threshold, no order) must not change selection.
    if (bDidOrderAction && rEvent.button == MouseButton_t::Left && !rEvent.bPressed
        && pClickedTile && IsExploredByPlayer_(worldX, worldY))
    {
        SetSelectedTile_(pClickedTile);
    }
    if (rInput.WasBombardRequested() && pControllable && rInput.GetBombardTarget())
    {
        TryBeginBombard_(*pControllable, *rInput.GetBombardTarget());
        return;
    }
    if (rInput.WasAttackRequested() && pControllable && rInput.GetAttackTarget())
    {
        TryBeginAttack_(*pControllable, *rInput.GetAttackTarget());
        return;
    }
    if (rInput.WasProbeActionRequested() && pControllable && rInput.GetProbeTarget())
    {
        TryOpenProbeActions_(*pControllable, *rInput.GetProbeTarget());
        return;
    }
    if (rInput.WasOrderAssigned() && pControllable)
    {
        m_rGameState.GetUnitOrderExecutor().Execute(*pControllable);
        SelectNextAvailableUnit_();
    }
}

// Without a sized explored map, every tile counts as explored.
bool WorldView::IsExploredByPlayer_(int worldX, int worldY) const
{
    const Faction* pPlayer = m_rGameState.GetPlayerFaction();
    if (!pPlayer || !pPlayer->GetExploredMap().IsSized())
    {
        return true;
    }
    return pPlayer->GetExploredMap().IsExplored(worldX, worldY);
}

void WorldView::SelectAtClick_(const Tile* pClickedTile, int worldX, int worldY)
{
    if (!IsExploredByPlayer_(worldX, worldY))
    {
        return;
    }
    SetSelectedTile_(pClickedTile);
    // Base takes priority over garrison selection: the base screen has its own unit stack
    // for picking units on the tile. Matches docs/architecture/ui-system.md.
    if (BaseManager* pBase = m_rGameState.FindBaseAt(worldX, worldY))
    {
        SetSelectedUnit_(nullptr, true);
        m_onOpenBase(*pBase);
        return;
    }
    // Unit pick uses IsUnitVisibleTo (fog / Conceal / contact reveal), not tile fog alone.
    SelectUnitAtTile_(worldX, worldY);
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

bool WorldView::TrySharedChord_(const KeyEvent_t& rEvent, Unit& rUnit)
{
    const bool bBombard = m_rHotkeys.Matches(HotkeyAction_t::Bombard, rEvent) && CanBombard(rUnit);
    const std::vector<std::string> projects = StartableTerraforms_(rEvent, rUnit);
    const std::size_t count = (bBombard ? 1 : 0) + projects.size();
    if (count == 0)
    {
        return false;
    }
    if (count > 1)
    {
        ThrowAmbiguousChord_(rEvent, bBombard, projects);
    }
    if (bBombard)
    {
        ToggleBombardTargeting_();
        return true;
    }
    if (m_rGameState.GetUnitOrderExecutor().TryStartTerraform(rUnit, projects.front(), m_rGameState))
    {
        SelectNextAvailableUnit_();
    }
    return true;
}

// Terraform projects bound to this chord that the unit could start now.
std::vector<std::string> WorldView::StartableTerraforms_(const KeyEvent_t& rEvent,
                                                         const Unit& rUnit) const
{
    std::vector<std::string> projects;
    if (!ResolveFlag(rUnit, RuleFlagId_t::Terraform))
    {
        return projects;
    }
    for (const TerraformHotkey_t& rBinding : m_rHotkeys.Terraform())
    {
        if (rBinding.chord.Matches(rEvent)
            && m_rGameState.GetUnitOrderExecutor().CanStartTerraformProject(rUnit,
                                                                            rBinding.projectId))
        {
            projects.push_back(rBinding.projectId);
        }
    }
    return projects;
}

void WorldView::ThrowAmbiguousChord_(const KeyEvent_t& rEvent, bool bBombard,
                                     const std::vector<std::string>& rProjects)
{
    const HotkeyChord_t chord{rEvent.key, rEvent.modifier.bCtrl, rEvent.modifier.bAlt,
                              rEvent.modifier.bShift};
    std::string message = DescribeHotkeyChord(chord) + " has more than one valid action:";
    if (bBombard)
    {
        message += " bombard";
    }
    for (const std::string& rProjectId : rProjects)
    {
        message += " ";
        message += rProjectId;
    }
    throw std::logic_error(message);
}

void WorldView::ToggleBombardTargeting_()
{
    if (m_bBombardTargeting)
    {
        ClearBombardTargeting_();
    }
    else
    {
        ClearAirdropTargeting_();
        m_bBombardTargeting = true;
    }
    m_bPresentationDirty = true;
}

bool WorldView::TryBeginBombard_(Unit& rAttacker, const Tile& rTargetTile)
{
    // Snapshot before the shot: it may destroy the targets whose names playback shows.
    std::shared_ptr<BombardPlayback_t> pPlayback = SnapshotBombardTargets_(rAttacker, rTargetTile);
    auto result = m_rGameState.GetUnitOrderExecutor().TryBombard(rAttacker, rTargetTile);
    if (!result)
    {
        return false;
    }
    pPlayback->combats = AsBombardPlayback_(std::move(result->combats));
    m_pWorldDisplay->SetPlaybackVisibleUnits(&pPlayback->playbackUnitIds);
    m_bPresentationDirty = true;
    ContinueBombardPlayback_(std::move(pPlayback), 0);
    return true;
}

// Names every unit on the target tile, and marks the ones the player cannot see so playback
// shows them for the duration.
std::shared_ptr<WorldView::BombardPlayback_t> WorldView::SnapshotBombardTargets_(
    const Unit& rAttacker, const Tile& rTargetTile) const
{
    auto pPlayback = std::make_shared<BombardPlayback_t>();
    pPlayback->pAttackerTile = &rAttacker.GetTile();
    pPlayback->pTargetTile = &rTargetTile;
    pPlayback->attackerName = rAttacker.GetDesign().GetName();
    const Faction* pPlayer = m_rGameState.GetPlayerFaction();
    const WorldMap& rMap = m_rGameState.GetWorldMap();
    const std::vector<Unit*>& rOccupants = rMap.GetUnitsOnTile(rTargetTile);
    const std::vector<Unit*>& rCargo = rMap.GetCargoOnTile(rTargetTile);
    for (const std::vector<Unit*>* pUnits : {&rOccupants, &rCargo})
    {
        for (Unit* pUnit : *pUnits)
        {
            if (!pUnit)
            {
                continue;
            }
            pPlayback->names[pUnit->GetUnitId()] = pUnit->GetDesign().GetName();
            if (pPlayer && !IsUnitVisibleTo(*pPlayer, *pUnit))
            {
                pPlayback->playbackUnitIds.insert(pUnit->GetUnitId());
            }
        }
    }
    return pPlayback;
}

// A shot that hit nothing still plays once.
std::vector<CombatResult_t> WorldView::AsBombardPlayback_(std::vector<CombatResult_t> combats)
{
    if (combats.empty())
    {
        combats.emplace_back();
    }
    for (CombatResult_t& rCombat : combats)
    {
        rCombat.bBombardPlayback = true;
    }
    return combats;
}

void WorldView::ContinueBombardPlayback_(std::shared_ptr<BombardPlayback_t> pPlayback,
                                         size_t index)
{
    if (!pPlayback || index >= pPlayback->combats.size())
    {
        m_pWorldDisplay->SetPlaybackVisibleUnits(nullptr);
        m_bPresentationDirty = true;
        SetSuppressDashboard(false);
        SelectNextAvailableUnit_();
        return;
    }

    const CombatResult_t& rCombat = pPlayback->combats[index];
    std::string defenderName = "Defender";
    if (const auto it = pPlayback->names.find(rCombat.defenderId); it != pPlayback->names.end())
    {
        defenderName = it->second;
    }

    SetSuppressDashboard(true);
    m_onOpenCombat(
        rCombat,
        *pPlayback->pAttackerTile,
        *pPlayback->pTargetTile,
        pPlayback->attackerName,
        defenderName,
        *m_pWorldDisplay,
        m_mapLayout,
        [this, pPlayback, index]() { ContinueBombardPlayback_(pPlayback, index + 1); });
}

void WorldView::TryOpenProbeActions_(Unit& rProbe, const Tile& rTargetTile)
{
    std::vector<std::pair<ProbeActionId_t, std::string>> actions =
        m_rGameState.GetProbeActions().ListAvailableProbeActions(rProbe, rTargetTile,
                                                                 m_rGameState);
    if (actions.empty())
    {
        return;
    }

    // The tile, not what is on it: the target may die while the popup is open, and TryProbeAction
    // re-resolves it.
    Unit* pProbe = &rProbe;
    const Tile* pTargetTile = &rTargetTile;

    std::vector<PopupChoice_t> choices;
    choices.reserve(actions.size());
    for (const auto& [id, rLabel] : actions)
    {
        choices.push_back({rLabel,
                           [this, pProbe, pTargetTile, id]
                           {
                               if (m_pSelectedUnit != pProbe)
                               {
                                   return;
                               }
                               const ProbeActionResult_t result =
                                   m_rGameState.GetProbeActions().TryProbeAction(
                                       *pProbe, id, *pTargetTile, m_rGameState, BuildingId_t{});
                               if (result.outcome != ProbeActionOutcome_t::Rejected)
                               {
                                   SelectNextAvailableUnit_();
                               }
                           }});
    }

    DismissOpenModals_();
    auto pPopup = std::make_unique<ListSelectorPopup>(
        "Probe Actions", "No actions available", std::move(choices),
        GetPopupLayout(), Style().listSelectorPopup, &m_rSprites);
    m_elements.push_back(std::move(pPopup));
}

std::string WorldView::FindUnitNameOnTile_(const Tile& rTile) const
{
    const std::vector<Unit*> units = m_rGameState.GetWorldMap().GetAllUnitsOnTile(rTile);
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

    const std::vector<Unit*> units = rWorldMap.GetAllUnitsOnTile(*pTile);
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
        if (pPlayer && !IsUnitVisibleTo(*pPlayer, *pUnit))
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

void WorldView::OpenDisbandMenu_(Unit& rUnit)
{
    Unit* pUnit = &rUnit;
    std::vector<PopupChoice_t> choices;
    choices.push_back({"Disband",
                       [this, pUnit]
                       {
                           if (m_pSelectedUnit == pUnit)
                           {
                               HandleDisbandChoice_(*pUnit);
                           }
                       }});
    choices.push_back({"Self Destruct",
                       [this, pUnit]
                       {
                           if (m_pSelectedUnit == pUnit)
                           {
                               ShowSelfDestructStub_();
                           }
                       }});
    choices.push_back({"Cancel", [] {}});

    DismissOpenModals_();
    auto pPopup = std::make_unique<ListSelectorPopup>(
        "Disband Units", "", std::move(choices),
        GetPopupLayout(), Style().listSelectorPopup, &m_rSprites);
    m_elements.push_back(std::move(pPopup));
}

void WorldView::HandleDisbandChoice_(Unit& rUnit)
{
    Faction& rFaction = rUnit.GetFaction();
    const std::optional<ScrapPayout_t> payout = rFaction.QuoteScrapUnit(rUnit);
    if (!payout)
    {
        DismissOpenModals_();
        auto pPopup = std::make_unique<NoticePopup>(
            GetPopupLayout(),
            "Disband",
            "This unit cannot be disbanded.",
            nullptr,
            &m_rSprites);
        m_elements.push_back(std::move(pPopup));
        return;
    }

    Unit* pUnit = &rUnit;
    DismissOpenModals_();
    auto pConfirm = MakeConfirmPopup(
        FormatScrapConfirm(*payout, rFaction),
        GetPopupLayout(),
        [this, pUnit]
        {
            if (m_pSelectedUnit != pUnit)
            {
                return;
            }
            HandleDisbandConfirmed_(*pUnit);
        },
        Style().listSelectorPopup);
    m_elements.push_back(std::move(pConfirm));
}

void WorldView::HandleDisbandConfirmed_(Unit& rUnit)
{
    Faction& rFaction = rUnit.GetFaction();
    if (!rFaction.QuoteScrapUnit(rUnit))
    {
        DismissOpenModals_();
        auto pPopup = std::make_unique<NoticePopup>(
            GetPopupLayout(),
            "Disband",
            "This unit cannot be disbanded.",
            nullptr,
            &m_rSprites);
        m_elements.push_back(std::move(pPopup));
        return;
    }

    rFaction.ScrapUnit(rUnit);
    SelectNextAvailableUnit_();
}

void WorldView::ShowSelfDestructStub_()
{
    DismissOpenModals_();
    auto pPopup = std::make_unique<NoticePopup>(
        GetPopupLayout(),
        "Self Destruct",
        "Self Destruct is not implemented.",
        nullptr,
        &m_rSprites);
    m_elements.push_back(std::move(pPopup));
}

void WorldView::ClearAirdropTargeting_()
{
    m_bAirdropTargeting = false;
}

void WorldView::ClearBombardTargeting_()
{
    m_bBombardTargeting = false;
}

void WorldView::DrawConsoleChrome_(Graphics& rGraphics) const
{
    const auto& console = Style().worldView;
    const WindowLayout_t& rConsole = m_consolePlacement.layout;
    // Start below the map so the inset strip (tower overlap) is not painted over.
    const float bandTop = m_mapLayout.y + m_mapLayout.height;
    const float bandHeight = (m_layout.y + m_layout.height) - bandTop;
    if (bandHeight > 0.0f)
    {
        rGraphics.DrawFilledRect(
            m_layout.x, bandTop, m_layout.width, bandHeight, console.consoleBackdropColor);
    }

    if (m_consolePlacement.sprite.empty() || !m_rSprites.Ensure(m_consolePlacement.sprite))
    {
        return;
    }
    rGraphics.DrawSprite(
        m_consolePlacement.sprite,
        rConsole.x + m_consolePlacement.spriteOffsetX,
        rConsole.y);
}

void WorldView::SyncTargetingCursor_(Graphics& rGraphics)
{
    const WorldDisplayStyle_t& rStyle = Style().worldDisplay;
    TargetingCursor_t want = TargetingCursor_t::None;
    const std::string* pPath = nullptr;
    unsigned int hotspotX = 0;
    unsigned int hotspotY = 0;
    if (m_bAirdropTargeting && !rStyle.airdropCursorPath.empty())
    {
        want = TargetingCursor_t::Airdrop;
        pPath = &rStyle.airdropCursorPath;
        hotspotX = rStyle.airdropCursorHotspotX;
        hotspotY = rStyle.airdropCursorHotspotY;
    }
    else if (m_bBombardTargeting && !rStyle.bombardCursorPath.empty())
    {
        want = TargetingCursor_t::Bombard;
        pPath = &rStyle.bombardCursorPath;
        hotspotX = rStyle.bombardCursorHotspotX;
        hotspotY = rStyle.bombardCursorHotspotY;
    }

    if (want == m_appliedTargetingCursor)
    {
        return;
    }
    if (m_appliedTargetingCursor != TargetingCursor_t::None)
    {
        rGraphics.ResetMouseCursor();
        m_appliedTargetingCursor = TargetingCursor_t::None;
    }
    if (want == TargetingCursor_t::None || pPath == nullptr)
    {
        return;
    }
    if (rGraphics.SetMouseCursor(*pPath, hotspotX, hotspotY))
    {
        m_appliedTargetingCursor = want;
    }
}

void WorldView::ShowAirdropNotice_(std::string message)
{
    DismissOpenModals_();
    auto pPopup = std::make_unique<NoticePopup>(
        GetPopupLayout(),
        "Airdrop",
        std::move(message),
        nullptr,
        &m_rSprites);
    m_elements.push_back(std::move(pPopup));
}

void WorldView::TryCommitAirdrop_(Unit& rUnit, const Tile& rDest)
{
    const auto result = m_rGameState.GetUnitOrderExecutor().TryAirdrop(rUnit, rDest);
    if (!result.Ok())
    {
        ShowAirdropNotice_(AirdropFailReasonMessage(result.failReason));
        m_bPresentationDirty = true;
        return;
    }

    ClearAirdropTargeting_();
    m_bPresentationDirty = true;
    if (!result.bMoverDestroyed)
    {
        SelectNextAvailableUnit_();
    }
}

} // namespace ac
