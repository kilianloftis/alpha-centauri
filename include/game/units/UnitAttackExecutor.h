#pragma once

#include "game/combat/AttackRules.h"
#include "game/combat/CombatResolver.h"
#include "game/combat/MoraleCalculator.h"
#include "game/diplomacy/DiplomaticTransitionRules.h"
#include "game/units/Unit.h"

#include <optional>
#include <random>
#include <string>
#include <vector>

namespace ac
{

class IUnitOrderWorld;
class MoveCostRules;
class StepEvaluator;
class Tile;
class TileEffectsContext;
class UnitOrderExecutor;
class WorldMap;

// Attacks and bombards: intercepts, scramblers, combat, promotion and the attack's cost.
class UnitAttackExecutor
{
public:
    // rOrders moves a scrambler to the defender before it fights.
    UnitAttackExecutor(const MoveCostRules& rMoveCosts, const StepEvaluator& rSteps,
                       WorldMap& rWorldMap, TileEffectsContext& rTileEffects,
                       const MoraleCalculator& rMorale, std::mt19937& rRng,
                       IUnitOrderWorld& rWorld, UnitOrderExecutor& rOrders);

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

private:
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
    void PromoteOnKill_(Unit& rAttacker, Unit& rDefender, const CombatResult_t& rResult);
    // Marks the attack, spends AttackMoveFragments, clears the order, and destroys a
    // SingleUse unit. Returns true when that expend destroyed rAttacker.
    bool SpendAttackAction_(Unit& rAttacker, AttackKind_t kind);
    // Last-defender casualties on a base tile. Returns true when the raid destroyed rAttacker.
    bool ApplyLastDefenderConquest_(Unit& rAttacker, const Tile& rDefenderTile);
    CombatResult_t ResolveBombardExchange_(Unit& rAttacker, Unit& rDefender,
                                           CombatEngagement_t engagement);
    // Assign a MoveOrder to an eligible scrambler and Execute until arrival (or failure).
    // Fills rOutPath with hops taken. Returns the combat defender (scrambler if arrived).
    Unit& ResolveScrambleDefender_(Unit& rAttacker,
                                   Unit& rOriginalDefender,
                                   std::vector<const Tile*>& rOutPath);
    // Moves rScrambler onto rDest, recording each hop. False when it did not arrive.
    bool ScrambleTo_(Unit& rScrambler, const Tile& rDest, std::vector<const Tile*>& rOutPath);

    WorldMap& m_rWorldMap;
    TileEffectsContext& m_rTileEffects;
    const MoraleCalculator& m_rMorale;
    std::mt19937& m_rRng;
    CombatResolver m_combat;
    IUnitOrderWorld& m_rWorld;
    UnitOrderExecutor& m_rOrders;
};

} // namespace ac
