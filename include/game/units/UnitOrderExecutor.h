#pragma once

#include "game/units/CombatResolver.h"
#include "game/units/MoraleCalculator.h"
#include "game/units/StepEvaluator.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <random>
#include <string>
#include <unordered_set>

namespace ac
{

class MoveCostCalculator;
class WorldMap;
class Tile;
class Pathfinder;
class BaseManager;
class GameState;
struct GameDataContext;

// Applies step/attack/order/use actions using shared MoveCostCalculator, StepEvaluator, and
// Pathfinder references. Path search and step legality are shared world queries owned by
// GameState; this class owns only the mutation logic. SingleUse expenditure lives here
// (ExpendIfSingleUse_), not in CombatResolver or FoundBaseRules.
class UnitOrderExecutor
{
public:
    UnitOrderExecutor(const MoveCostCalculator& rMoveCosts,
                      const StepEvaluator& rSteps,
                      WorldMap& rWorldMap,
                      TileEffectsContext& rTileEffects,
                      Pathfinder& rPathfinder,
                      const MoraleCalculator& rMorale,
                      std::mt19937& rRng);
    ~UnitOrderExecutor() = default;

    // Advance the unit's current order one pass. Returns Continue if the order remains,
    // Complete if it finished and the unit survives, Expended if SingleUse should be
    // DestroyUnit'd by the caller (PlayerActions under DeferDestruction).
    // Clears the unit's order whenever progress is not Continue.
    OrderProgress_t Execute(Unit& rUnit);

    // Apply one legal empty-tile step (MoveUnit + spend tile move-cost fragments). On
    // BlockedByOccupant / BlockedByZoc, contact-reveals the attributed blocking units.
    // Newly revealed hostiles (contact or fog) cancel a pending MoveOrder on rMover.
    // Fungus multi-turn charge progress is stored on rMoveOrder.
    bool TryStep(Unit& rMover, const Tile& rTo, MoveOrder_t& rMoveOrder);

    // Attack a hostile-occupied adjacent tile without moving onto it. Returns nullopt if not
    // adjacent, out of moves, or rTargetTile has no hostile unit visible to the attacker.
    // Otherwise resolves combat (HP / DestroyUnit) and returns the round-by-round result for
    // UI playback. SingleUse attackers return as destroyed after the caller (or this method)
    // runs DestroyUnit on OrderProgress_t::Expended.
    std::optional<CombatResult_t> TryAttack(Unit& rAttacker, const Tile& rTargetTile);

    // Found a base on the unit's tile. Requires FoundBase flag and a legal tile (spacing +
    // territory). onBaseCreated runs after CreateBase (e.g. EventBridge::WireBase). SingleUse
    // colony pods are DestroyUnit'd here on Expended. Returns the new base, or nullptr.
    BaseManager* TryFoundBase(Unit& rUnit, GameState& rGameState,
                              const GameDataContext& rDataContext,
                              std::function<void(BaseManager&)> onBaseCreated = {});

    // Begin a Former terraform project for improvementId. Spends energy up front and
    // assigns TerraformOrder_t. Returns false if ineligible.
    bool TryStartTerraform(Unit& rUnit, const std::string& improvementId, GameState& rGameState);

    // Hostile unit on rTile that rObserver's faction can actually see, or nullptr. This is
    // the visibility gate TryAttack applies before resolving combat, exposed so input
    // routing and AI can ask rather than re-derive it. Concealed occupants read as absent,
    // which is what keeps an order on their tile a move until a blocked step reveals them.
    Unit* FindVisibleHostileOnTile(const Unit& rObserver, const Tile& rTile) const;

private:
    // SingleUse outcome for a completed use-action. Does not destroy — callers (TryAttack /
    // TryFoundBase, or PlayerActions for Execute) must DestroyUnit on Expended.
    OrderProgress_t ExpendIfSingleUse_(const Unit& rUnit) const;

    OrderProgress_t Execute_(Unit& rUnit, MoveOrder_t& rOrder);
    OrderProgress_t Execute_(Unit& rUnit, HoldOrder_t& rOrder);
    OrderProgress_t Execute_(Unit& rUnit, HoldUntilHealedOrder_t& rOrder);
    OrderProgress_t Execute_(Unit& rUnit, HoldForTurnsOrder_t& rOrder);
    OrderProgress_t Execute_(Unit& rUnit, SupplyCrawlOrder_t& rOrder);
    OrderProgress_t Execute_(Unit& rUnit, TerraformOrder_t& rOrder);

    void RevealBlockingUnits_(Unit& rMover, const StepEvaluation_t& rEval);
    void EnterTile_(Unit& rMover, const Tile& rTo, int remainingAfter);
    bool SpendMovesAndEnter_(Unit& rMover, const Tile& rTo, MoveOrder_t& rMoveOrder);
    void CollectVisibleHostileIds_(const Unit& rObserver,
                                   std::unordered_set<UnitId_t>& rOut) const;
    bool HasNewlyVisibleHostile_(const Unit& rObserver,
                                 const std::unordered_set<UnitId_t>& rPreviouslyVisible) const;
    void CancelMoveOrderIfNewHostile_(Unit& rMover,
                                      const std::unordered_set<UnitId_t>& rPreviouslyVisible);

    const MoveCostCalculator& m_rMoveCosts;
    const StepEvaluator& m_rSteps;
    WorldMap& m_rWorldMap;
    TileEffectsContext& m_rTileEffects;
    Pathfinder& m_rPathfinder;
    const MoraleCalculator& m_rMorale;
    std::mt19937& m_rRng;
    CombatResolver m_combat;
};

} // namespace ac
