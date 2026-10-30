#pragma once

#include <vector>

namespace ac
{

class MoveCostRules;
class StepEvaluator;
class Tile;
class Unit;
class WorldMap;

// Withdrawal needs a faster combat unit alone on its tile, that did not attack this turn or
// last and holds no Hold order, with no air side and no PreventsDisengage on opponent or tile.
class DisengageRules
{
public:
    DisengageRules(const MoveCostRules& rMoveCosts,
                   const StepEvaluator& rSteps,
                   WorldMap& rWorldMap);

    bool CanDisengage(const Unit& rCandidate, const Unit& rOpponent) const;
    std::vector<const Tile*> CollectRetreatTiles(const Unit& rUnit) const;

private:
    const MoveCostRules& m_rMoveCosts;
    const StepEvaluator& m_rSteps;
    WorldMap& m_rWorldMap;
};

// A unit may withdraw once it has lost half the HP it started the fight with.
bool PastDisengageThreshold(const Unit& rUnit, int startHp);

} // namespace ac
