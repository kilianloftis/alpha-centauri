#pragma once

#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"

#include <optional>
#include <random>
#include <string>

namespace ac
{

class BaseManager;
class GameState;
class IUnitOrderWorld;
class Pathfinder;
class StepEvaluator;
class TerrainOperationRegistry;
class Tile;
class TileEffectsContext;
class UnitMoveExecutor;
class WorldMap;

// Carries out unit orders turn by turn, founds bases and starts terraforming, and spends
// SingleUse units.
class UnitOrderExecutor
{
public:
    UnitOrderExecutor(UnitMoveExecutor& rMoves, const StepEvaluator& rSteps, WorldMap& rWorldMap,
                      TileEffectsContext& rTileEffects, Pathfinder& rPathfinder,
                      const TerrainOperationRegistry& rTerrainOperations, std::mt19937& rRng,
                      IUnitOrderWorld& rWorld);

    // Advances the unit's order one pass, and clears the order once it ends.
    OrderProgress_t Execute(Unit& rUnit);

    // Drops turn-scoped orders (SkipTurn) when moves refresh.
    void OnTurnStart(Unit& rUnit);

    // Founds a base on the unit's tile, spending a SingleUse unit. nullptr when it may not.
    BaseManager* TryFoundBase(Unit& rUnit, GameState& rGameState);

    // Whether rUnit may start projectId (a buildable improvement or a terrain operation) on
    // its tile now, under the rules of the world it stands in.
    bool CanStartTerraformProject(const Unit& rUnit, const std::string& projectId) const;

    // Starts projectId, paying its energy up front. False when the unit may not.
    bool TryStartTerraform(Unit& rUnit, const std::string& projectId, GameState& rGameState);

private:
    OrderProgress_t Execute_(Unit& rUnit, MoveOrder_t& rOrder);
    // One step toward rDestination; nullopt while the move goes on.
    std::optional<OrderProgress_t> AdvanceMove_(Unit& rUnit, const Tile& rDestination);
    // No path to rDestination: steps into contact with what blocks it, if anything.
    OrderProgress_t StepTowardContact_(Unit& rUnit, const Tile& rDestination,
                                       MoveOrder_t& rOrder);
    // The move's next step enters territory the unit may not enter. A player unit keeps its
    // order and the world asks the player; any other unit drops the order.
    OrderProgress_t RefuseTerritoryEntry_(Unit& rUnit, const Tile& rNext);
    OrderProgress_t Execute_(Unit& rUnit, HoldOrder_t& rOrder);
    OrderProgress_t Execute_(Unit& rUnit, HoldUntilHealedOrder_t& rOrder);
    OrderProgress_t Execute_(Unit& rUnit, HoldForTurnsOrder_t& rOrder);
    OrderProgress_t Execute_(Unit& rUnit, SkipTurnOrder_t& rOrder);
    OrderProgress_t Execute_(Unit& rUnit, SupplyCrawlOrder_t& rOrder);
    OrderProgress_t Execute_(Unit& rUnit, TerraformOrder_t& rOrder);

    UnitMoveExecutor& m_rMoves;
    const StepEvaluator& m_rSteps;
    WorldMap& m_rWorldMap;
    TileEffectsContext& m_rTileEffects;
    Pathfinder& m_rPathfinder;
    const TerrainOperationRegistry& m_rTerrainOperations;
    std::mt19937& m_rRng;
    IUnitOrderWorld& m_rWorld;
};

} // namespace ac
