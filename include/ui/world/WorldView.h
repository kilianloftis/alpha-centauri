#pragma once

#include "ui/IGameView.h"
#include "ui/world/CameraInputController.h"
#include "ui/world/UnitOrderInputController.h"
#include "ui/world/WorldDisplay.h"
#include "input/Input.h"
#include <functional>
#include <memory>
#include <utility>

namespace ac
{

class GameState;
class BaseManager;
class Graphics;
class Unit;
class WorldMap;
class EndTurnButton;

class WorldView : public IGameView
{
public:
    using OpenBaseCallback_t = std::function<void(BaseManager&)>;

    WorldView(
        GameState& rGameState,
        const WorldMap& rWorldMap,
        WindowLayout_t layout,
        std::function<void()> onProcessTurn,
        std::function<void()> onRequestExit,
        OpenBaseCallback_t onOpenBase
    );

    void Render(Graphics& rGraphics) override;
    bool HandleKey(const KeyEvent_t& rEvent) override;
    void HandleMouse(const MouseEvent_t& rEvent) override;
    void UpdateCameraInput(bool bEnabled);

private:
    void Update_();
    void SelectUnitAtTile_(int tileX, int tileY);
    // Auto-cycle: fill empty selection, or advance past a unit that no longer needs orders.
    // Does not steal a manual selection of a unit that already moved / has an order.
    void SelectNextAvailableUnitIfNeeded_();
    // After an order is assigned — jump to the next unit that still needs orders.
    void SelectNextAvailableUnit_();
    Unit* GetControllableSelectedUnit_() const;
    bool PlayerUnitsNeedOrders_() const;
    static bool UnitRequiresOrders_(const Unit& rUnit);

    GameState& m_rGameState;
    const WindowLayout_t m_mapLayout;
    std::unique_ptr<WorldDisplay> m_pWorldDisplay;
    std::function<void()> m_onProcessTurn;
    std::function<void()> m_onRequestExit;
    OpenBaseCallback_t m_onOpenBase;

    Unit* m_pSelectedUnit = nullptr;
    // True after a map click selection; cleared when auto-cycling via GetNextAvailableUnit.
    bool m_bManualSelection = false;
    // Falling-edge detect for auto-advance when Pause at End of Turn is off.
    bool m_bHadUnitsNeedingOrders = false;

    std::unique_ptr<CameraInputController> m_pCameraInputController;
    std::unique_ptr<UnitOrderInputController> m_pUnitOrderInputController;
    EndTurnButton* m_pEndTurnButton = nullptr;
};

} // namespace ac
