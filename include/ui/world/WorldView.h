#pragma once

#include "game/units/CombatResolver.h"
#include "ui/IWorldView.h"
#include "ui/world/CameraInputController.h"
#include "ui/world/UnitOrderInputController.h"
#include "ui/world/WorldDisplay.h"
#include "input/Input.h"
#include <cstdint>
#include <functional>
#include <memory>
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

    WorldView(
        GameState& rGameState,
        const HotkeyConfig& rHotkeys,
        const WorldMap& rWorldMap,
        MapRenderer& rMapRenderer,
        WindowLayout_t layout,
        std::function<void()> onProcessTurn,
        std::function<void()> onRequestExit,
        OpenBaseCallback_t onOpenBase,
        OpenCombatCallback_t onOpenCombat,
        std::function<void()> onOpenCommlinks
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

    // Runs the auto end-turn callback if one was queued by the last UpdatePresentation pass
    // (Pause at End of Turn off, no units left needing orders). Called from UIManager::Update(),
    // between ProcessInput and Render, so turn advance never runs on the paint path.
    void ProcessPendingAutoEndTurn() override;

    // InteractionPresenter: camera focus and in-view modals for queue Front items.
    void CenterOnTile(int tileX, int tileY);
    void PushModal(std::unique_ptr<UIElement> pElement);
    WindowLayout_t GetPopupLayout() const;

private:
    void Update_();
    void SetSelectedUnit_(Unit* pUnit, bool bManualSelection);
    void SetSelectedTile_(const Tile* pTile);
    void SelectUnitAtTile_(int tileX, int tileY);
    // Auto-cycle: fill empty selection, or advance past a unit that no longer needs orders.
    // Does not steal a manual selection (empty tile browse, or a unit that already moved /
    // has an order).
    void SelectNextAvailableUnitIfNeeded_();
    // After an order/action — keep the selection if that unit still needs orders
    // (e.g. a short move with moves left); otherwise jump to the next unit that does.
    void SelectNextAvailableUnit_();
    // SMAC-style home path when auto-selected and this turn's out-of-fuel would be lethal.
    void TryAutoReturnLowFuel_(Unit& rUnit);
    Unit* GetControllableSelectedUnit_() const;
    bool PlayerUnitsNeedOrders_() const;
    static bool UnitRequiresOrders_(const Unit& rUnit);
    void TryBeginAttack_(Unit& rAttacker, const Tile& rTargetTile);
    // False when the shot is illegal. True after playback has been opened.
    bool TryBeginBombard_(Unit& rAttacker, const Tile& rTargetTile);
    // Bombard and former projects bound to this chord. Runs the one that is valid and
    // throws when more than one is.
    bool TrySharedChord_(const KeyEvent_t& rEvent, Unit& rUnit);
    void ToggleBombardTargeting_();
    struct BombardPlayback_t
    {
        std::vector<CombatResult_t> combats;
        std::unordered_map<UnitId_t, std::string> names;
        std::unordered_set<UnitId_t> playbackUnitIds;
        const Tile* pAttackerTile = nullptr;
        const Tile* pTargetTile = nullptr;
        std::string attackerName;
    };
    void ContinueBombardPlayback_(std::shared_ptr<BombardPlayback_t> pPlayback, size_t index);
    void TryOpenProbeActions_(Unit& rProbe, const Tile& rTargetTile);
    std::string FindUnitNameOnTile_(const Tile& rTile) const;
    void OpenDisbandMenu_(Unit& rUnit);
    void HandleDisbandChoice_(Unit& rUnit);
    void HandleDisbandConfirmed_(Unit& rUnit);
    void ShowSelfDestructStub_();
    void ClearAirdropTargeting_();
    void ClearBombardTargeting_();
    void SyncTargetingCursor_(Graphics& rGraphics);
    void TryCommitAirdrop_(Unit& rUnit, const Tile& rDest);
    void ShowAirdropNotice_(std::string message);

    GameState& m_rGameState;
    const HotkeyConfig& m_rHotkeys;
    const WindowLayout_t m_mapLayout;
    std::unique_ptr<WorldDisplay> m_pWorldDisplay;
    std::function<void()> m_onProcessTurn;
    std::function<void()> m_onRequestExit;
    OpenBaseCallback_t m_onOpenBase;
    OpenCombatCallback_t m_onOpenCombat;
    std::function<void()> m_onOpenCommlinks;

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

    // Snapshots used by UpdatePresentation to detect paint-relevant changes without input.
    Unit* m_pLastPresentedUnit = nullptr;
    const Tile* m_pLastPresentedTile = nullptr;
    bool m_bLastEndTurnReady = false;
    bool m_bLastHadPathPreview = false;
    size_t m_lastPathPreviewLength = 0;
    const Tile* m_pLastPathPreviewEnd = nullptr;
    int m_lastCameraX = 0;
    int m_lastCameraY = 0;
    int m_lastMissionYear = -1;
    int m_lastEnergy = -1;
    int m_lastResearch = -1;
    uint64_t m_lastExploredRevision = 0;
    uint64_t m_lastVisibleRevision = 0;
    uint64_t m_lastUnitRevision = 0;
    uint64_t m_lastAppearanceRevision = 0;

    std::unique_ptr<CameraInputController> m_pCameraInputController;
    std::unique_ptr<UnitOrderInputController> m_pUnitOrderInputController;
    SelectedUnitPanel* m_pSelectedUnitPanel = nullptr;
    LocationPanel* m_pLocationPanel = nullptr;
    InfoPanelElement* m_pInfoPanel = nullptr;
    UnitStackPanel* m_pUnitStackPanel = nullptr;
    EndTurnButton* m_pEndTurnButton = nullptr;
};

} // namespace ac
