#pragma once

#include <vector>

namespace ac
{

class Unit;
class WorldMap;

// Which side of a fight a roll / outcome refers to. Attacker initiated; Defender is the
// unit on the target tile.
enum class CombatSide_t
{
    Attacker,
    Defender,
};

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

// Equal rolls favour the defender. Psi combat deals the receiver's PsiDamage. A strike
// damages only the defender, and never below hpFloor.
CombatRoundOutcome_t ResolveCombatRound(const Unit& rAttacker, const Unit& rDefender,
                                        int attackRoll, int defenseRoll,
                                        CombatEngagement_t engagement, int hpFloor);

// A unit may withdraw once it has lost half the HP it started the fight with.
bool PastDisengageThreshold(const Unit& rUnit, int startHp);

// What collateral does to one other occupant of the defender's tile.
struct CollateralHit_t
{
    Unit* pOccupant = nullptr;
    // A MaxClamp that leaves collateral_susceptibility at 0 shields the occupant.
    bool bImmune = false;
    // A wild native is destroyed outright.
    bool bDestroyed = false;
    int damage = 0;
};

// One entry per other occupant of the defender's tile, in tile order.
std::vector<CollateralHit_t> PlanStackCollateral(const Unit& rAttacker, const Unit& rDefender,
                                                 const WorldMap& rWorldMap);

// Read after collateral lands: while NonCombatantsDestroyedWithoutCombatant is in force, the
// non-combatants left with no combatant beside them. Immune occupants count for neither.
std::vector<Unit*> NonCombatantsLeftUnprotected(const Unit& rAttacker, const Unit& rDefender,
                                                const WorldMap& rWorldMap,
                                                const std::vector<CollateralHit_t>& rPlan);

} // namespace ac
