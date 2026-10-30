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

    // Attacks rDefender on an adjacent tile without moving. nullopt unless rDefender is the
    // hostile FindAttackableHostileOnTile picks there; otherwise the result, for playback. An
    // intercept or scrambler can still change who fights.
    std::optional<CombatResult_t> TryAttack(Unit& rAttacker, Unit& rDefender);

    // One bombard shot, spending all remaining moves. nullopt when the unit cannot bombard.
    struct BombardResult_t
    {
        std::vector<CombatResult_t> combats;
        std::optional<std::string> destroyedImprovementId;
        bool bAttackerDestroyed = false;
    };
    std::optional<BombardResult_t> TryBombard(Unit& rAttacker, const Tile& rTargetTile);

private:
    // Combat with rDefender or a scrambler in its place, and the report of any capture.
    CombatResult_t FightDefender_(Unit& rAttacker, Unit& rDefender);
    CombatResult_t FireDuel_(Unit& rAttacker, Unit& rDuelTarget);
    // Strikes never hurt the attacker, so every target is struck.
    std::vector<CombatResult_t> FireStrikes_(Unit& rAttacker, const std::vector<Unit*>& rTargets);
    void ReportHostileAct_(const HostileAct_t& rAct);
    void ReportCapture_(const Unit& rAttacker, const CombatResult_t& rResult);
    // Returns the destroyed improvement's id.
    std::string DestroyRandomImprovement_(const Tile& rTile,
                                          const std::vector<std::string>& rCandidates);
    // Marks the attack, spends AttackMoveFragments, clears the order, and destroys a
    // SingleUse unit. Returns true when that expend destroyed rAttacker.
    bool SpendAttackAction_(Unit& rAttacker, AttackKind_t kind);
    // Last-defender casualties on a base tile. Returns true when the raid destroyed rAttacker.
    bool ApplyLastDefenderConquest_(Unit& rAttacker, const Tile& rDefenderTile);
    // Marks a duel or strike for bombard playback and keeps a surviving target from carrying on
    // with its order.
    CombatResult_t SettleBombardExchange_(Unit& rDefender, CombatResult_t combat);
    // Assign a MoveOrder to an eligible scrambler and Execute until arrival (or failure).
    // Fills rOutPath with hops taken. Returns the combat defender (scrambler if arrived).
    Unit& ResolveScrambleDefender_(Unit& rAttacker,
                                   Unit& rOriginalDefender,
                                   std::vector<const Tile*>& rOutPath);
    // Moves rScrambler onto rDest, recording each hop. False when it did not arrive.
    bool ScrambleTo_(Unit& rScrambler, const Tile& rDest, std::vector<const Tile*>& rOutPath);

    WorldMap& m_rWorldMap;
    TileEffectsContext& m_rTileEffects;
    std::mt19937& m_rRng;
    CombatResolver m_combat;
    IUnitOrderWorld& m_rWorld;
    UnitOrderExecutor& m_rOrders;
};

} // namespace ac
