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

// Attacks and bombards: intercepts, scramblers, combat and its aftermath, and the attack's cost.
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
    // After a fight or duel, never an intercept or a strike: collateral on rBattleTile when the
    // defender died there and promotion for the survivor, then the world's
    // ApplyFightConsequences. rBattleTile is read before the fight.
    void ApplyFightAftermath_(Unit& rAttacker, Unit& rDefender, const Tile& rBattleTile,
                              CombatResult_t& rResult);
    CombatResult_t FireDuel_(Unit& rAttacker, Unit& rDuelTarget);
    // Strikes never hurt the attacker, so every target is struck.
    std::vector<CombatResult_t> FireStrikes_(Unit& rAttacker, const std::vector<Unit*>& rTargets);
    // Returns the destroyed improvement's id.
    std::string DestroyRandomImprovement_(const Tile& rTile,
                                          const std::vector<std::string>& rCandidates);
    // A target that survives a duel or strike loses its order.
    void HaltBombardedTarget_(Unit& rTarget, const CombatResult_t& rCombat);
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
