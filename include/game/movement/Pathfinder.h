#pragma once

#include "game/movement/StepEvaluator.h"

#include <vector>

namespace ac
{

class MoveCostRules;
class Tile;
class Unit;
class WorldMap;

// Result of a cheapest-path search. tiles is ordered from the first step through the
// destination (origin excluded). Empty tiles with bReachable means already at destination.
struct Path_t
{
    std::vector<const Tile*> tiles;
    int totalCostFragments = 0;
    // Tiles on the route in territory the mover may not enter. Nonzero only when no route
    // avoids such territory; a move order stops at the first one (BlockedByTerritory).
    int forbiddenTerritoryTiles = 0;
    bool bReachable = false;
};

// Lowest-cost path under the mover's faction knowledge. Mutates nothing;
// UnitOrderExecutor re-plans after each step.
class Pathfinder
{
public:
    Pathfinder(const MoveCostRules& rMoveCosts,
               const StepEvaluator& rSteps,
               WorldMap& rWorldMap);

    // Recalculate whenever faction knowledge changes.
    Path_t FindPath(const Unit& rMover, const Tile& rDestination) const;

    // First tile of FindPath, or nullptr when unreachable / already at destination.
    const Tile* NextStep(const Unit& rMover, const Tile& rDestination) const;

    // The adjacent tile closest to rDestination that is legal or blocked by a unit, for
    // contact-reveal bumps when FindPath fails.
    const Tile* DesiredContactStep(const Unit& rMover, const Tile& rDestination) const;

    const WorldMap& GetWorldMap() const { return m_rWorldMap; }

private:
    const MoveCostRules& m_rMoveCosts;
    const StepEvaluator& m_rSteps;
    WorldMap& m_rWorldMap;
};

} // namespace ac
