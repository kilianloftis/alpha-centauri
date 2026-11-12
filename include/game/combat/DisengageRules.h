#pragma once

#include <optional>
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
// DisengageThreshold also requires a positive disengage_hp_limit on the candidate.
class DisengageRules
{
public:
    DisengageRules(const MoveCostRules& rMoveCosts,
                   const StepEvaluator& rSteps,
                   WorldMap& rWorldMap);

    bool CanDisengage(const Unit& rCandidate, const Unit& rOpponent) const;
    // HP the candidate must lose before its one-try retreat search; unset when ineligible or
    // disengage_hp_limit rounds to 0. ceil(startHp × limit) with 0.5 matches (HP+1)/2.
    std::optional<int> DisengageThreshold(const Unit& rCandidate, const Unit& rOpponent) const;
    std::vector<const Tile*> CollectRetreatTiles(const Unit& rUnit) const;

private:
    const MoveCostRules& m_rMoveCosts;
    const StepEvaluator& m_rSteps;
    WorldMap& m_rWorldMap;
};

} // namespace ac
