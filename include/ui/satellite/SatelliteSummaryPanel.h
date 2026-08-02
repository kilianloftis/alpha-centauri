#pragma once

#include "ui/UIElement.h"

namespace ac
{

class GameState;
class BuildingRegistry;

// Grid: orbital building types across the top, factions down the side, ownership counts in cells.
class SatelliteSummaryPanel : public UIElement
{
public:
    SatelliteSummaryPanel(GameState& rGameState,
                          const BuildingRegistry& rBuildings,
                          WindowLayout_t layout);

    void Render(Graphics& rGraphics) override;

private:
    GameState& m_rGameState;
    const BuildingRegistry& m_rBuildings;
};

} // namespace ac
