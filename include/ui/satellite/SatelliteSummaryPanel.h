#pragma once

#include "ui/UIElement.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace ac
{

class GameState;
class BuildingRegistry;
class Faction;
struct BuildingConfig_t;

// Grid: orbital building types across the top, factions down the side, ownership counts in cells.
class SatelliteSummaryPanel : public UIElement
{
public:
    SatelliteSummaryPanel(GameState& rGameState,
                          const BuildingRegistry& rBuildings,
                          WindowLayout_t layout);

    void Render(Graphics& rGraphics) override;

    // Rebuild the grid contents. Called on construction and by the view after an orbital
    // attack, which is the only thing that can change the census while this view is open.
    void Refresh();

private:
    GameState& m_rGameState;
    const BuildingRegistry& m_rBuildings;
    // Grid contents, not paint state: BuildOrbitalCensus walks every faction's bases and
    // buildings, and Render rebuilt all three of these on every frame.
    std::vector<const BuildingConfig_t*> m_orbitalTypes;
    std::vector<const Faction*> m_factions;
    std::unordered_map<std::string, int> m_counts;
};

} // namespace ac
