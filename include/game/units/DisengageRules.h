#pragma once

#include <vector>

namespace ac
{

class MoveCostCalculator;
class StepEvaluator;
class Tile;
class Unit;
class WorldMap;

// Pure query for mid-combat withdrawal eligibility and retreat destinations. Does not
// mutate units or roll dice — CombatResolver owns the HP threshold, tile pick, and move.
//
// CanDisengage is true when ALL of:
//   - rCandidate is a combat unit (resolved Attack > 0 or ForcesPsiCombat),
//   - rCandidate is strictly faster than rOpponent (Movement points),
//   - rCandidate is not stacked with other units on its tile,
//   - rCandidate did not attack on its current or previous turn,
//   - neither combatant is an air unit,
//   - rOpponent does not carry the PreventsDisengage flag (e.g. Comm Jammer),
//   - rCandidate holds no Hold-family order,
//   - rCandidate's tile has no feature carrying the PreventsDisengage flag (Base, Bunker, ...).
//
// CollectRetreatTiles returns adjacent tiles that are a legal step (terrain, occupants, no
// ZOC violation) and are not a full-cost fungus entry (roads / treat-fungus-as-road admit it).
class DisengageRules
{
public:
    DisengageRules(const MoveCostCalculator& rMoveCosts,
                   const StepEvaluator& rSteps,
                   WorldMap& rWorldMap);

    bool CanDisengage(const Unit& rCandidate, const Unit& rOpponent) const;
    std::vector<const Tile*> CollectRetreatTiles(const Unit& rUnit) const;

private:
    const MoveCostCalculator& m_rMoveCosts;
    const StepEvaluator& m_rSteps;
    WorldMap& m_rWorldMap;
};

} // namespace ac
