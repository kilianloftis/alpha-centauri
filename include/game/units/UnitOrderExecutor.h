#pragma once

#include "game/diplomacy/DiplomaticTransitionRules.h"
#include "game/movement/AirdropRules.h"
#include "game/combat/AttackRules.h"
#include "game/combat/CombatResolver.h"
#include "game/combat/MoraleCalculator.h"
#include "game/movement/StepEvaluator.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"

#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

namespace ac
{

class MoveCostRules;
class WorldMap;
class Tile;
class Pathfinder;
class BaseManager;
class GameState;
class IUnitOrderWorld;
class TerrainOperationRegistry;

// A native raider is consumed by the base it raids, so a step can end with the mover gone.
struct StepResult_t
{
    bool bEntered = false;
    bool bMoverDestroyed = false;
};

// Carries out unit orders and actions, and spends SingleUse units.
class UnitOrderExecutor
{
public:
    UnitOrderExecutor(const MoveCostRules& rMoveCosts,
                      const StepEvaluator& rSteps,
                      WorldMap& rWorldMap,
                      TileEffectsContext& rTileEffects,
                      Pathfinder& rPathfinder,
                      const MoraleCalculator& rMorale,
                      const TerrainOperationRegistry& rTerrainOperations,
                      std::mt19937& rRng,
                      IUnitOrderWorld& rWorld);

    // Advances the unit's order one pass, and clears the order once it ends.
    OrderProgress_t Execute(Unit& rUnit);

    // Drops turn-scoped orders (SkipTurn) when moves refresh.
    void OnTurnStart(Unit& rUnit);

    // One step onto rTo. A blocked step reveals its blockers, and a newly seen hostile cancels
    // a move order. Multi-turn fungus entry progress is kept on rMoveOrder.
    [[nodiscard]] StepResult_t TryStep(Unit& rMover, const Tile& rTo, MoveOrder_t& rMoveOrder);

    // Attacks a hostile on an adjacent tile without moving. nullopt when the attack is not
    // allowed; otherwise the result, for playback. An intercept or scrambler can change it.
    std::optional<CombatResult_t> TryAttack(Unit& rAttacker, const Tile& rTargetTile);

    // One bombard shot, spending all remaining moves. nullopt when the unit cannot bombard.
    struct BombardResult_t
    {
        std::vector<CombatResult_t> combats;
        std::optional<std::string> destroyedImprovementId;
        bool bAttackerDestroyed = false;
    };
    std::optional<BombardResult_t> TryBombard(Unit& rAttacker, const Tile& rTargetTile);

    // Founds a base on the unit's tile, spending a SingleUse unit. nullptr when it may not.
    BaseManager* TryFoundBase(Unit& rUnit, GameState& rGameState);

    // Whether rUnit may start projectId (a buildable improvement or a terrain operation) on
    // its tile now, under the rules of the world it stands in.
    bool CanStartTerraformProject(const Unit& rUnit, const std::string& projectId) const;

    // Starts projectId, paying its energy up front. False when the unit may not.
    bool TryStartTerraform(Unit& rUnit, const std::string& projectId, GameState& rGameState);

    // Drops rUnit onto rDest with landing damage. failReason says why it could not.
    struct AirdropActionResult_t
    {
        AirdropFailReason_t failReason = AirdropFailReason_t::None;
        bool bEntered = false;
        bool bMoverDestroyed = false;
        bool Ok() const { return failReason == AirdropFailReason_t::None && bEntered; }
    };
    [[nodiscard]] AirdropActionResult_t TryAirdrop(Unit& rUnit, const Tile& rDest);

private:
    // SingleUse outcome for a completed use-action. Does not destroy — callers (TryAttack /
    // TryFoundBase, or PlayerActions for Execute) must DestroyUnit on Expended.
    OrderProgress_t ExpendIfSingleUse_(const Unit& rUnit) const;

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

    // A ready intercept fights the attack in the defender's place; nullopt when none does.
    std::optional<CombatResult_t> TryIntercept_(Unit& rAttacker, Unit& rDefender);
    // Combat with rDefender or a scrambler in its place, then the attack's cost and any
    // last-defender casualties.
    CombatResult_t FightDefender_(Unit& rAttacker, Unit& rDefender);
    // The duel first, then a strike on each target, until the attacker falls.
    std::vector<CombatResult_t> FireBombard_(Unit& rAttacker, Unit* pDuelTarget,
                                             const std::vector<Unit*>& rStrikeTargets);
    void ReportHostileAct_(const HostileAct_t& rAct);
    // Returns the destroyed improvement's id.
    std::string DestroyRandomImprovement_(const Tile& rTile,
                                          const std::vector<std::string>& rCandidates);

    void RevealBlockingUnits_(Unit& rMover, const StepEvaluation_t& rEval);
    void PromoteOnKill_(Unit& rAttacker, Unit& rDefender, const CombatResult_t& rResult);
    // Marks the attack, spends AttackMoveFragments, clears the order, and destroys a
    // SingleUse unit. Returns true when that expend destroyed rAttacker.
    bool SpendAttackAction_(Unit& rAttacker, AttackKind_t kind);
    // Last-defender casualties on a base tile. Returns true when the raid destroyed rAttacker.
    bool ApplyLastDefenderConquest_(Unit& rAttacker, const Tile& rDefenderTile);
    CombatResult_t ResolveBombardExchange_(Unit& rAttacker, Unit& rDefender,
                                           CombatEngagement_t engagement);
    // Position only; caller spends moves (SpendMoveFragments) before enter.
    void EnterTile_(Unit& rMover, const Tile& rTo);
    // Returns false when the arrival destroyed rMover (native raid).
    bool ApplyArrivalEffects_(Unit& rMover);
    StepResult_t SpendMovesAndEnter_(Unit& rMover, const Tile& rTo, MoveOrder_t& rMoveOrder);
    void CollectVisibleHostileIds_(const Unit& rObserver,
                                   std::unordered_set<UnitId_t>& rOut) const;
    bool HasNewlyVisibleHostile_(const Unit& rObserver,
                                 const std::unordered_set<UnitId_t>& rPreviouslyVisible) const;
    void CancelMoveOrderIfNewHostile_(Unit& rMover,
                                      const std::unordered_set<UnitId_t>& rPreviouslyVisible);

    // Assign a MoveOrder to an eligible scrambler and Execute until arrival (or failure).
    // Fills rOutPath with hops taken. Returns the combat defender (scrambler if arrived).
    Unit& ResolveScrambleDefender_(Unit& rAttacker,
                                   Unit& rOriginalDefender,
                                   std::vector<const Tile*>& rOutPath);
    // Moves rScrambler onto rDest, recording each hop. False when it did not arrive.
    bool ScrambleTo_(Unit& rScrambler, const Tile& rDest, std::vector<const Tile*>& rOutPath);

    const MoveCostRules& m_rMoveCosts;
    const StepEvaluator& m_rSteps;
    WorldMap& m_rWorldMap;
    TileEffectsContext& m_rTileEffects;
    Pathfinder& m_rPathfinder;
    const MoraleCalculator& m_rMorale;
    const TerrainOperationRegistry& m_rTerrainOperations;
    std::mt19937& m_rRng;
    CombatResolver m_combat;
    IUnitOrderWorld& m_rWorld;
};

} // namespace ac
