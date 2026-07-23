#pragma once

#include "game/units/StepEvaluator.h"

#include <vector>

namespace ac
{

class MoveCostCalculator;
class Tile;
class Unit;
class WorldMap;

// Result of a cheapest-path search. tiles is ordered from the first step through the
// destination (origin excluded). Empty tiles with bReachable means already at destination.
struct Path_t
{
    std::vector<const Tile*> tiles;
    int totalCostFragments = 0;
    bool bReachable = false;
};

// Dijkstra search for the lowest fragment-cost path under the mover's faction knowledge
// (explored terrain + visible units). Does not mutate units or reveal — UnitOrderExecutor
// owns those concerns and re-queries after each step as fog / contact reveal updates.
class Pathfinder
{
public:
    Pathfinder(const MoveCostCalculator& rMoveCosts,
               const StepEvaluator& rSteps,
               WorldMap& rWorldMap);

    // Cheapest path from rMover's current tile to rDestination using CanPlanStep /
    // PlannedCostFragments. Recalculate whenever faction knowledge changes (e.g. after
    // each executed step reveals fog / hostiles).
    Path_t FindPath(const Unit& rMover, const Tile& rDestination) const;

    // First tile of FindPath, or nullptr when unreachable / already at destination.
    const Tile* NextStep(const Unit& rMover, const Tile& rDestination) const;

    // Among adjacent objective Legal / BlockedByOccupant / BlockedByZoc tiles, pick the one
    // that minimizes Chebyshev distance to rDestination. Used for contact-reveal bumps when
    // FindPath fails (planner thought the way was clear / blocked for other reasons).
    const Tile* DesiredContactStep(const Unit& rMover, const Tile& rDestination) const;

    const WorldMap& GetWorldMap() const { return m_rWorldMap; }

private:
    const MoveCostCalculator& m_rMoveCosts;
    const StepEvaluator& m_rSteps;
    WorldMap& m_rWorldMap;
};

} // namespace ac
