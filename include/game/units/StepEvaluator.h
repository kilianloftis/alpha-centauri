#pragma once

#include "game/units/MoveCostCalculator.h"
#include <vector>

namespace ac
{

class ImprovementRegistry;
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
    NoMoves,
    NotAdjacent,
};

struct StepEvaluation_t
{
    StepOutcome_t outcome = StepOutcome_t::Legal;
    // Populated for BlockedByOccupant (hostile / stacking units on rTo) and BlockedByZoc
    // (foreign projectors that make the step a ZOC→ZOC violation). Empty otherwise.
    std::vector<Unit*> blockingUnits;
};

// Pure step / pathfinding evaluation over bound world + move-cost state. Does not mutate
// units or reveal — those are UnitOrderExecutor::TryStep / TryAttack concerns.
class StepEvaluator
{
public:
    StepEvaluator(const ImprovementRegistry& rImprovements,
                  WorldMap& rWorldMap,
                  const TileEffectsContext& rTileEffects);

    WorldMap& GetWorldMap() const;
    const MoveCostCalculator& GetMoveCosts() const;

    // True if rTile is in hostile ZOC for rMover (Chebyshev-1 around a qualifying foreign unit).
    bool IsTileInHostileZoc(const Unit& rMover, const Tile& rTile) const;

    // True when moving from rFrom to rTo would illegally transit through hostile ZOC
    // (ZOC -> ZOC). Attack is not a move — see UnitOrderExecutor::TryAttack.
    bool IsZocViolation(const Unit& rMover, const Tile& rFrom, const Tile& rTo) const;

    // Destination has at least one unit from another faction.
    bool HasHostileUnit(const Unit& rMover, const Tile& rTile) const;

    // Destination has at least one hostile unit visible to rMover's faction.
    bool HasVisibleHostileUnit(const Unit& rMover, const Tile& rTile) const;

    // Pure step rules over objective world state (visibility / reveal are not consulted).
    StepEvaluation_t EvaluateStep(const Unit& rMover, const Tile& rTo) const;

    // True when EvaluateStep reports Legal.
    bool CanStep(const Unit& rMover, const Tile& rTo) const;

    // Temporary next-step seam (real pathfinding later). Among adjacent tiles that pass
    // CanStep, pick the one that minimizes Chebyshev distance to rDestination.
    const Tile* ProposeNextStep(const Unit& rMover, const Tile& rDestination) const;

    // Like ProposeNextStep, but also considers tiles blocked only by occupants or ZOC.
    const Tile* ProposeDesiredStep(const Unit& rMover, const Tile& rDestination) const;

private:
    MoveCostCalculator m_moveCosts;
    WorldMap& m_rWorldMap;
    const TileEffectsContext& m_rTileEffects;
};

} // namespace ac
