#pragma once

#include "game/units/CombatStrength.h"
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
    // Playback draws the bombard placeholder on the target tile, including a shot with
    // no rounds. Melee hit flash stays off while this is set.
    bool bBombardPlayback = false;
    // Hops the scrambled defender walked before combat (origin excluded). Empty when no
    // scramble fired. UI can play these tile-by-tile before combat rounds.
    std::vector<const Tile*> scramblePath;
};

// Resolves SMAC-style firefight rounds: each side rolls [0, strength), higher roll wins the
// round (ties → defender), winner deals damage until one unit is destroyed — or either side
// disengages. Roll pools come from ResolveCombatStrength. Withdrawal eligibility and retreat
// destinations come from DisengageRules. This class owns the half-HP threshold, the chance
// roll, round ordering, random tile pick, MoveUnit, and DestroyUnit. After a round, the side
// that just took damage is checked first, then the other. A killed defender's stack splash
// is ApplyStackCollateral. GrantPlanetPearls pays for that defender, and for wild natives
// the splash destroys, before DestroyUnit.
//
enum class CombatEngagement_t
{
    Standard,
    // One round. An attacker loss records the roll and nominal damage and does not
    // change attacker HP. Defender HP cannot fall below the bombard floor, and a unit
    // already under that floor is not healed.
    ArtilleryStrike,
    // Full fight, no disengage. The defender is rated with Attack. Either side can reach 0.
    ArtilleryDuel,
};

struct CombatResolveOptions_t
{
    CombatEngagement_t engagement = CombatEngagement_t::Standard;
};

// RNG is injected (typically GameState's shared stream) so combat, promotion, and probe
// rolls can share one deterministic sequence.
class CombatResolver
{
public:
    static constexpr int k_combatStrengthScale = k_CombatStrengthScale;
    // Placeholder until weapon / reactor damage tables exist.
    static constexpr int k_roundDamage = 1;

    CombatResolver(const MoveCostCalculator& rMoveCosts,
                   const StepEvaluator& rSteps,
                   WorldMap& rWorldMap,
                   const TileEffectsContext& rTileEffects,
                   const MoraleCalculator& rMorale,
                   std::mt19937& rRng);

    // Applies HP each round, moves a unit on disengage, and DestroyUnit when a side
    // reaches 0. A killed defender also runs ApplyStackCollateral and GrantPlanetPearls.
    CombatResult_t Resolve(Unit& rAttacker, Unit& rDefender)
    {
        return Resolve(rAttacker, rDefender, {});
    }
    CombatResult_t Resolve(Unit& rAttacker, Unit& rDefender,
                           const CombatResolveOptions_t& rOptions);

private:
    int Roll_(int strength) const;
    int BombardHpFloor_(const Unit& rDefender) const;
    // If eligible and at the HP threshold with a retreat tile, moves rCandidate and records
    // the disengage on rResult. Returns true when combat should end.
    bool TryDisengage_(Unit& rCandidate, CombatSide_t side, int startHp,
                       bool bMayDisengage, CombatResult_t& rResult);

    DisengageRules m_disengage;
    WorldMap& m_rWorldMap;
    const TileEffectsContext& m_rTileEffects;
    const MoraleCalculator& m_rMorale;
    // Non-const reference: Roll_ is const so Resolve stays const-correct for callers.
    std::mt19937& m_rRng;
};

} // namespace ac
