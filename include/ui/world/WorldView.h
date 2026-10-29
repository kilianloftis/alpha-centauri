#pragma once

#include "game/base/BaseTypes.h"
#include "game/buildings/BuildingConfig.h"
#include "game/combat/CombatResolver.h"
#include "game/probes/ProbeActionConfig.h"
#include "ui/IWorldView.h"
#include "ui/world/CameraInputController.h"
#include "ui/world/UnitOrderInputController.h"
#include "ui/world/WorldDisplay.h"
#include "input/Input.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ac
{

class GameState;
class HotkeyConfig;
class BaseManager;
class Graphics;
class Unit;
class WorldMap;
class EndTurnButton;
class InfoPanelElement;
class LocationPanel;
class SelectedUnitPanel;
class Tile;
class MapRenderer;
class SpriteLibrary;
class UnitStackPanel;

class WorldView : public IWorldView
{
public:
    using OpenBaseCallback_t = std::function<void(BaseManager&)>;
    // Pushes CombatView. WorldView supplies display/map layout and an onFinished that
    // restores dashboard selection after playback.
    using OpenCombatCallback_t = std::function<void(
        CombatResult_t result,
        const Tile& rAttackerTile,
        const Tile& rDefenderTile,
        std::string attackerName,
        std::string defenderName,
        WorldDisplay& rWorldDisplay,
        WindowLayout_t mapLayout,
        std::function<void()> onFinished)>;

    // What the world view asks its host to do.
    struct Callbacks_t
    {
        std::function<void()> onProcessTurn;
        std::function<void()> onRequestExit;
        OpenBaseCallback_t onOpenBase;
        OpenCombatCallback_t onOpenCombat;
        std::function<void()> onOpenCommlinks;
    };

    WorldView(
        GameState& rGameState,
        const HotkeyConfig& rHotkeys,
        const WorldMap& rWorldMap,
        MapRenderer& rMapRenderer,
        SpriteLibrary& rSprites,
        WindowLayout_t layout,
        Callbacks_t callbacks
    );

    void Render(Graphics& rGraphics) override;
    bool HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouse(const MouseEvent_t& rEvent) override;
    bool UpdateCameraInput(bool bEnabled,
                           std::optional<MousePosition_t> mousePosition) override;
    void UpdatePresentation() override;
    bool ConsumePresentationDirty() override;

    // While CombatView is up, skip drawing the normal dashboard (combat panels cover it).
    void SetSuppressDashboard(bool bSuppress);

    // Runs a queued auto end-turn, from UIManager::Update, never the paint path.
    void ProcessPendingAutoEndTurn() override;

    // InteractionPresenter: camera focus and in-view modals for queue Front items.
    void CenterOnTile(int tileX, int tileY);
    void PushModal(std::unique_ptr<UIElement> pElement);

private:
    // Everything a repaint depends on that can change without input.
    struct PresentationSnapshot_t
    {
        const Unit* pSelectedUnit = nullptr;
        const Tile* pSelectedTile = nullptr;
        bool bEndTurnReady = false;
        bool bHadPathPreview = false;
        size_t pathPreviewLength = 0;
        const Tile* pPathPreviewEnd = nullptr;
        int cameraX = 0;
        int cameraY = 0;
        int missionYear = -1;
        int energy = -1;
        int research = -1;
        uint64_t exploredRevision = 0;
        uint64_t visibleRevision = 0;
        uint64_t unitRevision = 0;
        uint64_t appearanceRevision = 0;

        bool operator==(const PresentationSnapshot_t&) const = default;
    };

    void AddConsolePanels_(MapRenderer& rMapRenderer);
    void AddMinimapAndEndTurn_(MapRenderer& rMapRenderer);
    void WatchSelectionDepartures_();
    void DeselectIfSelected_(Unit& rUnit);
    // Keeps the element alive in m_elements and hands back the panel for direct calls.
    template <typename T>
    T& Adopt_(std::unique_ptr<T> pElement)
    {
        T& rElement = *pElement;
        m_elements.push_back(std::move(pElement));
        return rElement;
    }
    void Update_();
    void RefreshInfoPanel_();
    std::vector<Unit*> VisibleStackUnits_() const;
    // The End Turn highlight, and the auto end-turn latch when pause-at-end is off.
    void RefreshEndTurnState_();
    PresentationSnapshot_t CapturePresentationSnapshot_() const;
    // The player chose pUnit (or an empty tile): browsing, and skip-turn gives way.
    void PickUnit_(Unit* pUnit);
    // The view moved on to pUnit by itself, and the camera follows.
    void CycleToUnit_(Unit* pUnit);
    // Returns whether the selection changed.
    bool ChangeSelectedUnit_(Unit* pUnit);
    void SetSelectedTile_(const Tile* pTile);
    void SelectUnitAtTile_(int tileX, int tileY);
    // Fills an empty selection, or moves past a unit that no longer needs orders; never overrides
    // a manual selection.
    void SelectNextAvailableUnitIfNeeded_();
    // After an order/action — keep the selection if that unit still needs orders
    // (e.g. a short move with moves left); otherwise jump to the next unit that does.
    void SelectNextAvailableUnit_();
    // SMAC-style home path when auto-selected and this turn's out-of-fuel would be lethal.
    void TryAutoReturnLowFuel_(Unit& rUnit);
    Unit* GetControllableSelectedUnit_() const;
    bool PlayerUnitsNeedOrders_() const;
    void TryBeginAttack_(Unit& rAttacker, const Tile& rTargetTile);
    void ExecuteAttack_(Unit& rAttacker, const Tile& rTargetTile);
    // False when the shot is illegal. True after playback has been opened.
    bool TryBeginBombard_(Unit& rAttacker, const Tile& rTargetTile);
    // Bombard and former projects bound to this chord. Runs the one that is valid and
    // throws when more than one is.
    bool TrySharedChord_(const KeyEvent_t& rEvent, Unit& rUnit);
    std::vector<std::string> StartableTerraforms_(const KeyEvent_t& rEvent,
                                                  const Unit& rUnit) const;
    // A chord valid for two actions on one unit is a hotkey config defect.
    [[noreturn]] static void ThrowAmbiguousChord_(const KeyEvent_t& rEvent, bool bBombard,
                                                  const std::vector<std::string>& rProjects);
    void ToggleBombardTargeting_();
    bool ApplyOrderInput_(Unit& rControllable);
    void OpenSupplyCrawlMenu_(Unit& rUnit);
    void FoundBaseWith_(Unit& rUnit);
    void Detonate_(Unit& rUnit);
    void ToggleAirdropTargeting_();
    // A hold order may link the unit to its base's on-hold effects; then the next unit.
    void OnOrderAssigned_(Unit* pControllable);
    // Cancel, End Turn and Next Unit.
    bool HandleChromeKey_(const KeyEvent_t& rEvent);
    // Cancel leaves targeting first, and only exits when nothing is being targeted.
    void CancelTargetingOrExit_();
    // Topmost chrome element under a press takes it.
    bool RouteClickToChrome_(const MouseEvent_t& rEvent);
    bool TryTargetingClick_(const MouseEvent_t& rEvent, Unit* pControllable,
                            const Tile* pClickedTile);
    // Acts on what the order controller just recognised from the click.
    void ApplyOrderClick_(const MouseEvent_t& rEvent, Unit* pControllable,
                          const Tile* pClickedTile, int worldX, int worldY);
    bool IsExploredByPlayer_(int worldX, int worldY) const;
    // Selects the clicked tile, then its base or its visible unit.
    void SelectAtClick_(const Tile* pClickedTile, int worldX, int worldY);
    struct BombardPlayback_t
    {
        std::vector<CombatResult_t> combats;
        std::unordered_map<UnitId_t, std::string> names;
        std::unordered_set<UnitId_t> playbackUnitIds;
        const Tile* pAttackerTile = nullptr;
        const Tile* pTargetTile = nullptr;
        std::string attackerName;
    };
    std::shared_ptr<BombardPlayback_t> SnapshotBombardTargets_(const Unit& rAttacker,
                                                               const Tile& rTargetTile) const;
    static std::vector<CombatResult_t> AsBombardPlayback_(std::vector<CombatResult_t> combats);
    void ContinueBombardPlayback_(std::shared_ptr<BombardPlayback_t> pPlayback, size_t index);
    void TryOpenProbeActions_(Unit& rProbe, const Tile& rTargetTile);
    void OpenSabotageFacilityPicker_(Unit& rProbe, const Tile& rTargetTile);
    // Opens the frame picker when the probe may implicate someone, else runs the action.
    void CommitProbeAction_(Unit& rProbe, ProbeActionId_t actionId, const Tile& rTargetTile,
                            const BuildingId_t& facilityId);
    void OpenFramePicker_(Unit& rProbe, ProbeActionId_t actionId, const Tile& rTargetTile,
                          const BuildingId_t& facilityId,
                          const std::vector<FactionId_t>& rCandidates);
    std::function<void()> FrameChoiceAction_(Unit& rProbe, ProbeActionId_t actionId,
                                             const Tile& rTargetTile,
                                             const BuildingId_t& facilityId,
                                             std::optional<FactionId_t> framedId);
    void RunProbeAction_(Unit& rProbe, ProbeActionId_t actionId, const Tile& rTargetTile,
                         const BuildingId_t& facilityId, std::optional<FactionId_t> framedId);
    std::string FindUnitNameOnTile_(const Tile& rTile) const;
    void OpenDisbandMenu_(Unit& rUnit);
    void HandleDisbandChoice_(Unit& rUnit);
    void HandleDisbandConfirmed_(Unit& rUnit);
    void ShowSelfDestructStub_();
    void ClearAirdropTargeting_();
    void ClearBombardTargeting_();
    void SyncTargetingCursor_(Graphics& rGraphics);
    void DrawConsoleChrome_(Graphics& rGraphics) const;
    void TryCommitAirdrop_(Unit& rUnit, const Tile& rDest);
    void ShowAirdropNotice_(std::string message);

    GameState& m_rGameState;
    const HotkeyConfig& m_rHotkeys;
    SpriteLibrary& m_rSprites;
    const FixedPlacement_t m_consolePlacement;
    const WindowLayout_t m_mapLayout;
    std::unique_ptr<WorldDisplay> m_pWorldDisplay;
    Callbacks_t m_callbacks;

    Unit* m_pSelectedUnit = nullptr;
    const Tile* m_pSelectedTile = nullptr;
    // True after a map click selection; cleared when auto-cycling via GetNextAvailableUnit.
    bool m_bManualSelection = false;
    // Falling-edge detect for auto-advance when Pause at End of Turn is off.
    bool m_bHadUnitsNeedingOrders = false;
    bool m_bSuppressDashboard = false;
    // Set by UpdatePresentation; consumed by UIManager::Update for skip-redraw.
    bool m_bPendingAutoEndTurn = false;
    bool m_bPresentationDirty = true;
    bool m_bAirdropTargeting = false;
    bool m_bBombardTargeting = false;
    // Which targeting cursor is installed, so a mode switch replaces it.
    enum class TargetingCursor_t
    {
        None,
        Airdrop,
        Bombard
    };
    TargetingCursor_t m_appliedTargetingCursor = TargetingCursor_t::None;

    PresentationSnapshot_t m_lastPresented;

    std::unique_ptr<CameraInputController> m_pCameraInputController;
    std::unique_ptr<UnitOrderInputController> m_pUnitOrderInputController;
    SelectedUnitPanel* m_pSelectedUnitPanel = nullptr;
    LocationPanel* m_pLocationPanel = nullptr;
    InfoPanelElement* m_pInfoPanel = nullptr;
    UnitStackPanel* m_pUnitStackPanel = nullptr;
    EndTurnButton* m_pEndTurnButton = nullptr;
};

} // namespace ac
