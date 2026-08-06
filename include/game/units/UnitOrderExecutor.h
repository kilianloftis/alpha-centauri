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
class IUnitOrderWorld;
struct GameDataContext;

// Outcome of one applied step. A native raider is consumed by the base it raids, so a step
// can legally end with the mover gone; bMoverDestroyed is the signal that no caller may
// touch rMover again.
struct StepResult_t
{
    bool bEntered = false;
    bool bMoverDestroyed = false;
};

// Applies step/attack/order/use actions using shared MoveCostCalculator, StepEvaluator, and
// Pathfinder references. Path search and step legality are shared world queries owned by
// GameState; this class owns only the mutation logic. SingleUse expenditure lives here
// (ExpendIfSingleUse_), not in CombatResolver or FoundBaseRules.
class UnitOrderExecutor
{
public:
    // pWorld supplies the session-scoped queries (base lookup, intercept, conquest) that the
    // map and pathfinder cannot answer; GameState passes itself. Movement-only harnesses may
    // pass nullptr, which disables intercept and base conquest.
    UnitOrderExecutor(const MoveCostCalculator& rMoveCosts,
                      const StepEvaluator& rSteps,
                      WorldMap& rWorldMap,
                      TileEffectsContext& rTileEffects,
                      Pathfinder& rPathfinder,
                      const MoraleCalculator& rMorale,
                      std::mt19937& rRng,
                      IUnitOrderWorld* pWorld = nullptr);
    ~UnitOrderExecutor() = default;

    // Required for base conquest ownership transfer; Engine / tests set after construction,
    // matching DiplomaticActionExecutor — GameState is built before the data context binds.
    void SetGameDataContext(const GameDataContext& rGameData) { m_pGameData = &rGameData; }

    // Advance the unit's current order one pass. Returns Continue if the order remains,
    // Complete if it finished and the unit survives, Expended if SingleUse should be
    // DestroyUnit'd by the caller (PlayerActions under DeferDestruction), or UnitDestroyed
    // if the unit died advancing the order and is already gone.
    // Clears the unit's order whenever progress is not Continue.
    // When world + GameDataContext are bound, entering an undefended foreign base may
    // capture it or trigger a native raid.
    OrderProgress_t Execute(Unit& rUnit);

    // Apply one legal empty-tile step (MoveUnit + spend tile move-cost fragments). On
    // BlockedByOccupant / BlockedByZoc, contact-reveals the attributed blocking units.
    // Newly revealed hostiles (contact or fog) cancel a pending MoveOrder on rMover.
    // Fungus multi-turn charge progress is stored on rMoveOrder.
    // Stop using rMover when the result reports bMoverDestroyed.
    [[nodiscard]] StepResult_t TryStep(Unit& rMover, const Tile& rTo, MoveOrder_t& rMoveOrder);

    // Attack a hostile-occupied adjacent tile without moving onto it. Costs one movement
    // point; neither side changes tile. Returns nullopt when FindAttackableHostileOnTile
    // denies the declare (moves / adjacency / visibility / CanAttackTile). Otherwise
    // resolves combat (HP / DestroyUnit) and returns the round-by-round result for UI
    // playback. SingleUse attackers return as destroyed after the caller (or this method)
    // runs DestroyUnit on OrderProgress_t::Expended.
    // When world is bound, ready InterceptAttempt effects may destroy the attacker before
    // CombatResolver runs. After the last garrison dies on a base tile, last-defender
    // casualties / adjacent native raid apply when GameDataContext is bound — ownership
    // transfer still requires a later enter-tile order while moves remain.
    std::optional<CombatResult_t> TryAttack(Unit& rAttacker, const Tile& rTargetTile);

    // Found a base on the unit's tile. Requires FoundBase flag and a legal tile (spacing +
    // territory). Observers hang off Faction::OnBaseAdded, which CreateBase fires — callers do
    // not wire anything (see EventBridge::WireBase). SingleUse colony pods are DestroyUnit'd
    // here on Expended. Returns the new base, or nullptr.
    BaseManager* TryFoundBase(Unit& rUnit, GameState& rGameState,
                              const GameDataContext& rDataContext);

    // Begin a Former terraform project for improvementId. Spends energy up front and
    // assigns TerraformOrder_t. Returns false if ineligible.
    bool TryStartTerraform(Unit& rUnit, const std::string& improvementId, GameState& rGameState);

    // Forwards to AttackRules::FindVisibleHostileOnTile (visibility + embark-in-base
    // targeting). Input/AI use this; declare-attack legality is FindAttackableHostileOnTile.
    Unit* FindVisibleHostileOnTile(const Unit& rObserver, const Tile& rTile) const;

    // Attach rPassenger to the first boardable transport on its tile (L key / UI).
    bool TryAttachToTransport(Unit& rPassenger, bool bRefuelOnAttach = false);

    // Landing / stranded path: attach with refuel when possible.
    bool TryAutoAttachWhenMustLand(Unit& rPassenger);

    // Air carrier: unload all passengers onto the current tile (Shift+U).
    bool TryUnloadTransport(Unit& rCarrier);

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
    // Position + move-cost only; the arrival side effects below are applied by the caller.
    void EnterTile_(Unit& rMover, const Tile& rTo, int remainingAfter);
    // Returns false when the arrival destroyed rMover (native raid).
    bool ApplyArrivalEffects_(Unit& rMover, bool bWasEmbarked);
    StepResult_t SpendMovesAndEnter_(Unit& rMover, const Tile& rTo, MoveOrder_t& rMoveOrder);
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
    const GameDataContext* m_pGameData = nullptr;
    IUnitOrderWorld* const m_pWorld;
};

} // namespace ac
