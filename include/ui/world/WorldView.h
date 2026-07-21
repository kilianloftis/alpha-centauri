#pragma once

#include "game/units/CombatResolver.h"
#include "ui/IGameView.h"
#include "ui/world/CameraInputController.h"
#include "ui/world/UnitOrderInputController.h"
#include "ui/world/WorldDisplay.h"
#include "input/Input.h"
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace ac
{

class GameState;
class BaseManager;
class Graphics;
class Unit;
class WorldMap;
class EndTurnButton;
class InfoPanelElement;
class LocationPanel;
class SelectedUnitPanel;
class Tile;
class UnitStackPanel;

class WorldView : public IGameView
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
        const WorldMap& rWorldMap,
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
    void UpdateCameraInput(bool bEnabled);

    // While CombatView is up, skip drawing the normal dashboard (combat panels cover it).
    void SetSuppressDashboard(bool bSuppress);

private:
    void Update_();
    void SetSelectedUnit_(Unit* pUnit, bool bManualSelection);
    void SetSelectedTile_(const Tile* pTile);
    void SelectUnitAtTile_(int tileX, int tileY);
    // Auto-cycle: fill empty selection, or advance past a unit that no longer needs orders.
    // Does not steal a manual selection (empty tile browse, or a unit that already moved /
    // has an order).
    void SelectNextAvailableUnitIfNeeded_();
    // After an order is assigned — jump to the next unit that still needs orders.
    void SelectNextAvailableUnit_();
    Unit* GetControllableSelectedUnit_() const;
    bool PlayerUnitsNeedOrders_() const;
    static bool UnitRequiresOrders_(const Unit& rUnit);
    void TryBeginAttack_(Unit& rAttacker, const Tile& rTargetTile);
    std::string FindUnitNameOnTile_(const Tile& rTile) const;

    GameState& m_rGameState;
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

    std::unique_ptr<CameraInputController> m_pCameraInputController;
    std::unique_ptr<UnitOrderInputController> m_pUnitOrderInputController;
    SelectedUnitPanel* m_pSelectedUnitPanel = nullptr;
    LocationPanel* m_pLocationPanel = nullptr;
    InfoPanelElement* m_pInfoPanel = nullptr;
    UnitStackPanel* m_pUnitStackPanel = nullptr;
    EndTurnButton* m_pEndTurnButton = nullptr;
};

} // namespace ac
