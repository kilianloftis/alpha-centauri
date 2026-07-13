#pragma once

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

// Whether rProjector (a foreign unit) exerts zone of control that applies to rSubject.
// Air projectors affect land and sea subjects; sea/land projectors affect their own
// domain only. Air subjects never match those rules; IgnoreZoneOfControl also exempts.
bool UnitExertsZocOn(const Unit& rProjector, const Unit& rSubject);

// True if rTile is in hostile ZOC for rMover (Chebyshev-1 around a qualifying foreign unit).
bool IsTileInHostileZoc(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap);

// True when moving from rFrom to rTo would illegally transit through hostile ZOC
// (ZOC -> ZOC). Attack is not a move — see TryAttack.
bool IsZocViolation(const Unit& rMover, const Tile& rFrom, const Tile& rTo,
                    const WorldMap& rWorldMap);

// Destination has at least one unit from another faction.
bool HasHostileUnit(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap);

// Destination has at least one hostile unit visible to rMover's faction.
bool HasVisibleHostileUnit(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap,
                           const TileEffectsContext& rTileEffects);

// Whether rMover may enter rTile for its chassis domain (air any; sea water; land land).
bool CanEnterTileTerrain(const Unit& rMover, const Tile& rTile);

// Pure step rules over objective world state (visibility / reveal are not consulted).
// Move affordability uses tile move-cost fragments from rImprovements.
StepEvaluation_t EvaluateStep(const Unit& rMover, const Tile& rTo, const WorldMap& rWorldMap,
                              const ImprovementRegistry& rImprovements);

// True when EvaluateStep reports Legal.
bool CanStep(const Unit& rMover, const Tile& rTo, const WorldMap& rWorldMap,
             const ImprovementRegistry& rImprovements);

// Combat placeholder: no damage yet. Clears remaining moves and any active order.
void ResolveCombatStub(Unit& rAttacker);

// Attack a hostile-occupied adjacent tile without moving onto it. Returns false if not
// adjacent, out of moves, or rTargetTile has no hostile unit visible to the attacker.
bool TryAttack(Unit& rAttacker, const Tile& rTargetTile, const WorldMap& rWorldMap,
               const TileEffectsContext& rTileEffects);

// Apply one legal empty-tile step (TryMoveUnit + spend tile move-cost fragments). On
// BlockedByOccupant / BlockedByZoc, contact-reveals the attributed blocking units to the
// mover's faction.
bool TryStep(Unit& rMover, const Tile& rTo, WorldMap& rWorldMap,
             const ImprovementRegistry& rImprovements);

// Temporary next-step seam (real pathfinding later). Among adjacent tiles that pass
// CanStep, pick the one that minimizes Chebyshev distance to rDestination. Recalculate
// every call. Returns nullptr if already there or no improving legal step exists.
const Tile* ProposeNextStep(const Unit& rMover, const Tile& rDestination,
                            const WorldMap& rWorldMap,
                            const ImprovementRegistry& rImprovements);

// Like ProposeNextStep, but also considers tiles blocked only by occupants or ZOC — used
// to identify the bump target when the legal path is blocked (TryStep then reveals).
const Tile* ProposeDesiredStep(const Unit& rMover, const Tile& rDestination,
                               const WorldMap& rWorldMap,
                               const ImprovementRegistry& rImprovements);

} // namespace ac
