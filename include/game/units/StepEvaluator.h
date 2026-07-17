#pragma once

#include <vector>

namespace ac
{

class Tile;
class TileEffectsContext;
class Unit;
class WorldMap;

// Objective step-check result. Rules attribute blockers when the outcome is an occupant
// or ZOC block (hidden units still exert ZoC and occupy tiles — visibility is a Try* concern).
enum class StepOutcome_t
{
    Legal,
    BlockedByOccupant,
    BlockedByZoc,
    BlockedByTerrain,
    NotAdjacent,
};

struct StepEvaluation_t
{
    StepOutcome_t outcome = StepOutcome_t::Legal;
    // Populated for BlockedByOccupant (hostile / stacking units on rTo) and BlockedByZoc
    // (foreign projectors that make the step a ZOC→ZOC violation). Empty otherwise.
    std::vector<Unit*> blockingUnits;
};

// Pure step / pathfinding evaluation over bound world state. Does not mutate units or
// reveal — those are UnitOrderExecutor::TryStep / TryAttack concerns. Does not consult
// remaining move fragments; callers that gate on moves do so separately.
class StepEvaluator
{
public:
    StepEvaluator(WorldMap& rWorldMap, const TileEffectsContext& rTileEffects);

    // True if rTile is in hostile ZOC for rMover (Chebyshev-1 around a qualifying foreign unit).
    bool IsTileInHostileZoc(const Unit& rMover, const Tile& rTile) const;

    // True when moving from rFrom to rTo would illegally transit through hostile ZOC
    // (ZOC -> ZOC). Exempt when rTo holds a friendly unit or base (SMAC). Attack is not
    // a move — see UnitOrderExecutor::TryAttack.
    bool IsZocViolation(const Unit& rMover, const Tile& rFrom, const Tile& rTo) const;

    // Destination has at least one unit from another faction.
    bool HasHostileUnit(const Unit& rMover, const Tile& rTile) const;

    // Destination has at least one hostile unit visible to rMover's faction.
    bool HasVisibleHostileUnit(const Unit& rMover, const Tile& rTile) const;

    // Whether the edge rFrom → rTo is legal for rMover over objective world state.
    // Visibility / reveal / remaining moves are not consulted.
    StepEvaluation_t EvaluateStep(const Unit& rMover, const Tile& rFrom, const Tile& rTo) const;

    // True when EvaluateStep reports Legal.
    bool CanStep(const Unit& rMover, const Tile& rFrom, const Tile& rTo) const;

    // Path-planning edge check using only information known to rMover's faction: explored
    // terrain (shroud assumes domain-passable) and units visible to that faction. Concealed /
    // fogged hostiles and their ZOC are ignored until revealed.
    StepEvaluation_t EvaluatePlannedStep(const Unit& rMover, const Tile& rFrom,
                                         const Tile& rTo) const;

    // True when EvaluatePlannedStep reports Legal.
    bool CanPlanStep(const Unit& rMover, const Tile& rFrom, const Tile& rTo) const;

private:
    enum class Knowledge_t
    {
        Objective,
        FactionKnown,
    };

    StepEvaluation_t EvaluateStep_(const Unit& rMover, const Tile& rFrom, const Tile& rTo,
                                   Knowledge_t knowledge) const;

    bool IsTileInHostileZoc_(const Unit& rMover, const Tile& rTile, Knowledge_t knowledge) const;
    bool IsZocViolation_(const Unit& rMover, const Tile& rFrom, const Tile& rTo,
                         Knowledge_t knowledge) const;
    bool CanEnterTerrain_(const Unit& rMover, const Tile& rTile, Knowledge_t knowledge) const;
    bool UnitCountsForPlanner_(const Unit& rMover, const Unit& rOther,
                               Knowledge_t knowledge) const;

    WorldMap& m_rWorldMap;
    const TileEffectsContext& m_rTileEffects;
};

} // namespace ac
