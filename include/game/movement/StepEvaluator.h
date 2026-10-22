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
    // Physically enterable, but the territory owner's status with the mover forbids entry.
    // Path planning does not report it; only the step itself does.
    BlockedByTerritory,
    NotAdjacent,
};

struct StepEvaluation_t
{
    StepOutcome_t outcome = StepOutcome_t::Legal;
    // Populated for BlockedByOccupant (hostile / stacking units on rTo) and BlockedByZoc
    // (foreign projectors that make the step a ZOC→ZOC violation). Empty otherwise.
    std::vector<Unit*> blockingUnits;
};

// Step and path evaluation over world state. Mutates nothing and ignores remaining moves.
class StepEvaluator
{
public:
    StepEvaluator(WorldMap& rWorldMap, const TileEffectsContext& rTileEffects);

    // True if rTile is in hostile ZOC for rMover (Chebyshev-1 around a qualifying foreign unit).
    bool IsTileInHostileZoc(const Unit& rMover, const Tile& rTile) const;

    // Moving ZOC to ZOC, unless rTo holds a friendly unit or base (SMAC). Attacking is not a move.
    bool IsZocViolation(const Unit& rMover, const Tile& rFrom, const Tile& rTo) const;



    // Whether the edge rFrom → rTo is legal for rMover over objective world state.
    // Visibility / reveal / remaining moves are not consulted.
    StepEvaluation_t EvaluateStep(const Unit& rMover, const Tile& rFrom, const Tile& rTo) const;

    // True when EvaluateStep reports Legal.
    bool CanStep(const Unit& rMover, const Tile& rFrom, const Tile& rTo) const;

    // Uses only what rMover's faction knows: shroud passes, and unseen hostiles and their ZOC are
    // ignored.
    StepEvaluation_t EvaluatePlannedStep(const Unit& rMover, const Tile& rFrom,
                                         const Tile& rTo) const;

    // True when EvaluatePlannedStep reports Legal.
    bool CanPlanStep(const Unit& rMover, const Tile& rFrom, const Tile& rTo) const;

    // Whether rMover may enter rTile as far as its faction knows, so the planner can reject a
    // domain-mismatched goal without flooding.
    bool CanPlanEnterTerrain(const Unit& rMover, const Tile& rTile) const;

    // Whether rTile lies in territory rMover's faction may not enter under its current
    // diplomatic status with the owner.
    bool IsForbiddenTerritory(const Unit& rMover, const Tile& rTile) const;

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
