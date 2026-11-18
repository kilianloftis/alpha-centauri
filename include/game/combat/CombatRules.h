#pragma once

#include "game/effects/ActiveEffect.h"

namespace ac
{

class MoraleCalculator;
class Unit;

// Which side of a fight a roll / outcome refers to. Attacker initiated; Defender is the
// unit on the target tile.
enum class CombatSide_t
{
    Attacker,
    Defender,
};

// Which exchange the strengths are resolved for (ResolveCombatStrength).
enum class CombatEngagement_t
{
    // CombatResolver::Resolve.
    Standard,
    // CombatResolver::Strike: one round that never hurts the attacker.
    ArtilleryStrike,
    // CombatResolver::ResolveDuel: the defender is rated with Attack.
    ArtilleryDuel,
};

// Roll-pool scale applied to a resolved attack or defense rating.
inline constexpr int k_CombatStrengthScale = 0x100;

// What one fight is decided by, fixed before the first round. attackStrength and
// defenseStrength are the resolved ratings after k_CombatStrengthScale; each round damage is the
// HP that side loses per round it loses.
struct CombatStrength_t
{
    int attackStrength = 0;
    int defenseStrength = 0;
    int attackerRoundDamage = 0;
    int defenderRoundDamage = 0;
};

// What one kind of fight reads: the value stats each side rates on, the damage a round's winner
// deals, and the multiplier the loser takes it at. Both kinds share the Attack / Defense
// multipliers.
struct CombatKind_t
{
    CombatRatingStats_t attack;
    CombatRatingStats_t defense;
    StatId_t damage;
    StatId_t damageTaken;
};

inline constexpr CombatKind_t k_ConventionalCombat{
    {StatId_t::ConventionalAttack, StatId_t::Attack},
    {StatId_t::ConventionalDefense, StatId_t::Defense},
    StatId_t::ConventionalDamage,
    StatId_t::ConventionalDamageTaken,
};

inline constexpr CombatKind_t k_PsiCombat{
    {StatId_t::PsiAttack, StatId_t::Attack},
    {StatId_t::PsiDefense, StatId_t::Defense},
    StatId_t::PsiDamage,
    StatId_t::PsiDamageTaken,
};

// The fight is psi by engagement: Standard when the attacker forces psi combat or the defender
// forces psi defense; ArtilleryStrike when the attacker forces psi combat; ArtilleryDuel when
// either side forces psi combat (weapon vs weapon). Each side's rating is its value stat for the
// kind of fight times its Attack / Defense multiplier; the defender rates on attack in a duel.
// The partial-move penalty and the difficulty handicap scale the attacker's whole-number
// strength. A side's round damage is the other side's damage times its own damage taken, rounded
// and at least 1.
CombatStrength_t ResolveCombatStrength(const Unit& rAttacker, const Unit& rDefender,
                                       const MoraleCalculator& rMorale,
                                       CombatEngagement_t engagement);

struct CombatRoundOutcome_t
{
    CombatSide_t winner = CombatSide_t::Defender;
    // Recorded for playback even when a strike leaves the attacker untouched.
    int damage = 0;
    int attackerHpLoss = 0;
    int defenderHpLoss = 0;
};

// One round of a fight. Equal rolls favour the defender; the loser takes its round damage.
CombatRoundOutcome_t ResolveCombatRound(const CombatStrength_t& rStrength, int attackRoll,
                                        int defenseRoll);

// An artillery strike's one round: as ResolveCombatRound, but the attacker is never hurt and the
// defender never drops below hpFloor (one already under it is not healed).
CombatRoundOutcome_t ResolveStrikeRound(const CombatStrength_t& rStrength, const Unit& rDefender,
                                        int attackRoll, int defenseRoll, int hpFloor);

// HP a strike must leave on rTarget: bombard_min_hp_percent of its HitPoints, from the area
// effects on its tile, rounded up.
int BombardHpFloor(const Unit& rTarget);

// Reduced attacker:defender ratio of strength × hits-to-kill (Thinker combat_odds_fix).
// docs/thinker/terranx-map.md.
struct CombatOdds_t
{
    int attackerOdds = 1;
    int defenderOdds = 1;
};

CombatOdds_t ComputeCombatOdds(const Unit& rAttacker, const Unit& rDefender,
                               CombatEngagement_t engagement);

// Defender share of ComputeCombatOdds(..., Standard). SelectDefender ranks by this.
double DefenderWinChance(const Unit& rAttacker, const Unit& rDefender);

} // namespace ac
