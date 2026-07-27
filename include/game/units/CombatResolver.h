#pragma once

#include "game/units/DisengageRules.h"
#include "game/units/MoraleCalculator.h"
#include "game/units/Unit.h"

#include <cstdint>
#include <random>
#include <vector>

namespace ac
{

class MoveCostCalculator;
class StepEvaluator;
class Tile;
class TileEffectsContext;
class WorldMap;

// Which side of a fight a roll / outcome refers to. Attacker initiated; Defender is the
// unit on the target tile.
enum class CombatSide_t
{
    Attacker,
    Defender,
};

// One exchange of rolls. Damage is applied to the loser; ties award the round to the
// defender (attacker takes the hit). hpAfter values are post-damage snapshots for UI replay.
struct CombatRound_t
{
    int attackRoll = 0;
    int defenseRoll = 0;
    CombatSide_t roundWinner = CombatSide_t::Defender;
    int damage = 1;
    int attackerHpAfter = 0;
    int defenderHpAfter = 0;
};

// Full fight outcome. rounds is ordered chronologically so the UI can present each exchange
// at a digestible rate without re-rolling. HP changes, retreat moves, and DestroyUnit run
// during Resolve; the recorded rounds remain valid for playback after either unit is gone.
struct CombatResult_t
{
    UnitId_t attackerId = 0;
    UnitId_t defenderId = 0;
    // Resolved attack / defense ratings after * k_combatStrengthScale (pre-roll pools).
    int attackStrength = 0;
    int defenseStrength = 0;
    // True when either combatant carries ForcesPsiCombat. In psi combat, strengths start
    // at 1 and ignore additive weapon/armour values; round damage uses receiver PsiDamage.
    bool bPsiCombat = false;
    std::vector<CombatRound_t> rounds;
    // The side that remains on the field: the survivor, or the opponent of a unit that
    // disengaged. Attacker when the defender was destroyed or disengaged; Defender otherwise.
    CombatSide_t victor = CombatSide_t::Defender;
    bool bAttackerDestroyed = false;
    bool bDefenderDestroyed = false;
    // Whichever side withdrew mid-combat (see DisengageRules). The retreat move to
    // pRetreatTile has already happened; the UI plays it after the last round.
    bool bAttackerDisengaged = false;
    bool bDefenderDisengaged = false;
    const Tile* pRetreatTile = nullptr;
};

// Resolves SMAC-style firefight rounds: each side rolls [0, strength), higher roll wins the
// round (ties → defender), winner deals damage until one unit is destroyed — or either side
// disengages. Strength is the fully resolved attack or defense rating multiplied by
// k_combatStrengthScale. Defense also folds in ResolveTileDefenseMultiplier on the
// defender's tile.
//
// Withdrawal eligibility and retreat destinations are queried from DisengageRules. This
// class owns the half-HP threshold, round ordering, random tile pick, MoveUnit, and
// DestroyUnit. After a round, the side that just took damage is checked first, then the
// other.
class CombatResolver
{
public:
    static constexpr int k_combatStrengthScale = 0x100;
    // Placeholder until weapon / reactor damage tables exist.
    static constexpr int k_roundDamage = 1;

    CombatResolver(const MoveCostCalculator& rMoveCosts,
                   const StepEvaluator& rSteps,
                   WorldMap& rWorldMap,
                   const TileEffectsContext& rTileEffects,
                   const MoraleConfig_t& rMorale);
    CombatResolver(const MoveCostCalculator& rMoveCosts,
                   const StepEvaluator& rSteps,
                   WorldMap& rWorldMap,
                   const TileEffectsContext& rTileEffects,
                   const MoraleConfig_t& rMorale,
                   uint32_t seed);

    // Applies HP each round, moves a unit on disengage, and DestroyUnit when a side
    // reaches 0. Attack uses EffectContext_t{defender tile, Attacker}; defense uses
    // {defender tile, Defender} so IsDefending / Base conditions apply.
    CombatResult_t Resolve(Unit& rAttacker, Unit& rDefender);

    // Shared with promotion rolls in UnitOrderExecutor::TryAttack.
    std::mt19937& GetRng() const { return m_rng; }

private:
    int Roll_(int strength) const;
    // If eligible and at the HP threshold with a retreat tile, moves rCandidate and records
    // the disengage on rResult. Returns true when combat should end.
    bool TryDisengage_(Unit& rCandidate, CombatSide_t side, int startHp,
                       bool bMayDisengage, CombatResult_t& rResult);

    DisengageRules m_disengage;
    WorldMap& m_rWorldMap;
    const TileEffectsContext& m_rTileEffects;
    MoraleCalculator m_morale;
    // mutable: Roll_ / GetRng are const so Resolve can stay const-correct for callers.
    mutable std::mt19937 m_rng;
};

} // namespace ac
