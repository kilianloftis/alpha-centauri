#pragma once

namespace ac
{

class MoraleCalculator;
class TileEffectsContext;
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

// Roll pools for one fight. attackStrength and defenseStrength are the resolved ratings
// after k_CombatStrengthScale. bPsiCombat is true when either side forces psi combat.
struct CombatStrength_t
{
    int attackStrength = 0;
    int defenseStrength = 0;
    bool bPsiCombat = false;
};

// Attack and defence pools; psi ratings start at 1. The defender uses Defense (Attack in a duel)
// times its tile's multiplier; under one movement point, attack scales by the fraction left.
CombatStrength_t ResolveCombatStrength(const Unit& rAttacker, const Unit& rDefender,
                                       const TileEffectsContext& rTileEffects,
                                       const MoraleCalculator& rMorale,
                                       CombatEngagement_t engagement);

// Placeholder until weapon / reactor damage tables exist.
inline constexpr int k_CombatRoundDamage = 1;

struct CombatRoundOutcome_t
{
    CombatSide_t winner = CombatSide_t::Defender;
    // Recorded for playback even when a strike leaves the attacker untouched.
    int damage = 0;
    int attackerHpLoss = 0;
    int defenderHpLoss = 0;
};

// One round of a fight. Equal rolls favour the defender. Psi combat deals the receiver's
// PsiDamage.
CombatRoundOutcome_t ResolveCombatRound(const Unit& rAttacker, const Unit& rDefender,
                                        int attackRoll, int defenseRoll);

// An artillery strike's one round: as ResolveCombatRound, but the attacker is never hurt and the
// defender never drops below hpFloor (one already under it is not healed).
CombatRoundOutcome_t ResolveStrikeRound(const Unit& rAttacker, const Unit& rDefender,
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
