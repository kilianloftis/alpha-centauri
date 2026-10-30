#pragma once

#include "game/base/BaseTypes.h"
#include "game/combat/CombatRules.h"
#include "game/combat/DisengageRules.h"
#include "game/combat/MoraleCalculator.h"
#include "game/units/Unit.h"

#include <cstdint>
#include <optional>
#include <random>
#include <vector>

namespace ac
{

class MoveCostRules;
class StepEvaluator;
class Tile;
class TileEffectsContext;
class WorldMap;

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

// The whole fight, rounds in order for playback; they stay valid after either unit is gone.
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
    // A Marine Detachment boarding took the defender's tile mid-fight; nothing was destroyed.
    bool bDefenderBoarded = false;
    // False when the defender was captured_on_defeat: killing it earns no promotion.
    bool bKillEarnsPromotion = true;
    // The defender's owner when the fight began, from whom any capture was taken.
    FactionId_t defenderFactionId = 0;
    // Units the attacker took: the boarded tile, or defeated units captured instead of destroyed.
    // A captured defender still counts as destroyed for the fight's outcome.
    std::vector<UnitId_t> capturedUnitIds;
    // Playback draws the bombard placeholder on the target tile, including a shot with
    // no rounds. Melee hit flash stays off while this is set.
    bool bBombardPlayback = false;
    // Hops the scrambled defender walked before combat (origin excluded). Empty when no
    // scramble fired. UI can play these tile-by-tile before combat rounds.
    std::vector<const Tile*> scramblePath;
};

// Resolves fights and bombard shots. A fight rolls each side's [0, strength) pool until a unit dies
// or the fight ends early; the rounds only decide, and each ending then applies itself. The RNG is
// shared so combat, promotion and probe rolls are one stream.
class CombatResolver
{
public:
    static constexpr int k_combatStrengthScale = k_CombatStrengthScale;

    CombatResolver(const MoveCostRules& rMoveCosts,
                   const StepEvaluator& rSteps,
                   WorldMap& rWorldMap,
                   const TileEffectsContext& rTileEffects,
                   const MoraleCalculator& rMorale,
                   std::mt19937& rRng);

    // A melee fight: rounds until a unit dies, withdraws (disengage_chance) or is boarded
    // (board_chance). A killed defender also runs ApplyStackCollateral; every unit the attacker
    // kills goes through RemoveDefeatedUnit. The survivor of a kill or boarding may then be
    // promoted.
    CombatResult_t Resolve(Unit& rAttacker, Unit& rDefender);
    // Two artillery units trade fire until one falls; neither withdraws or boards. The defender
    // is rated with Attack. The survivor may be promoted, as after Resolve.
    CombatResult_t ResolveDuel(Unit& rAttacker, Unit& rDefender);
    // One bombard shot: the attacker is never hurt and the target stops at its bombard HP floor.
    // A killed target is destroyed outright: never captured, no promotion, no collateral.
    CombatResult_t Strike(Unit& rAttacker, Unit& rTarget);

private:
    // One side of the fight, as the withdrawal check needs it.
    struct Combatant_t
    {
        Unit& rUnit;
        CombatSide_t side;
        int startHp;
        bool bMayDisengage;
    };

    // How a fight may end before a death, decided before the first round.
    struct FightExits_t
    {
        bool bAttackerMayDisengage = false;
        bool bDefenderMayDisengage = false;
        // HP the defender must lose before boarding rolls start; unset when it cannot be boarded.
        std::optional<int> boardingHpToLose;
    };

    enum class FightEndKind_t
    {
        Death,
        Withdrawal,
        Boarding,
    };

    struct FightEnd_t
    {
        FightEndKind_t kind = FightEndKind_t::Death;
        // Withdrawal only: who withdrew, and to which tile.
        CombatSide_t withdrawingSide = CombatSide_t::Defender;
        const Tile* pRetreatTile = nullptr;
    };

    // Ids, the defender's owner, promotion eligibility and both strengths.
    CombatResult_t BeginResult_(const Unit& rAttacker, const Unit& rDefender,
                                CombatEngagement_t engagement) const;
    CombatResult_t Fight_(Unit& rAttacker, Unit& rDefender, CombatEngagement_t engagement,
                          const FightExits_t& rExits);
    // Plays rounds into rResult until a unit dies or EarlyEnd_ ends the fight. Changes only HP.
    FightEnd_t FightRounds_(const Combatant_t& rAttacker, const Combatant_t& rDefender,
                            std::optional<int> boardingHpToLose, CombatResult_t& rResult);
    // After a round both survived: a boarding when the defender was hit, then a withdrawal by the
    // side just hit (it may have newly crossed the threshold), then by the other.
    std::optional<FightEnd_t> EarlyEnd_(const Combatant_t& rAttacker, const Combatant_t& rDefender,
                                        const CombatRound_t& rRound,
                                        std::optional<int>& rBoardingHpToLose);
    // Eligible, past the threshold, with a retreat tile and a won disengage_chance roll: the
    // withdrawal and its tile.
    std::optional<FightEnd_t> ChooseWithdrawal_(const Combatant_t& rCandidate);
    // Wears rHpToLose down by the round's damage and, once it is spent, rolls board_chance. Unset
    // rHpToLose never boards.
    bool BoardingRollWon_(const Unit& rAttacker, const Unit& rDefender, int damage,
                          std::optional<int>& rHpToLose) const;
    // Applies the ending the rounds chose.
    void Finish_(Unit& rAttacker, Unit& rDefender, const FightEnd_t& rEnd,
                 CombatResult_t& rResult);
    // Rolls promotion for the side PromotionCandidate names, if any.
    void PromoteSurvivor_(Unit& rAttacker, Unit& rDefender, const CombatResult_t& rResult);
    // Moves rWithdrawing to its retreat tile; withdrawing concedes the fight.
    void Withdraw_(Unit& rWithdrawing, const FightEnd_t& rEnd, CombatResult_t& rResult);
    // Captures the defender's tile and records it on rResult.
    void Board_(Unit& rAttacker, const Unit& rDefender, CombatResult_t& rResult);
    // Removes whichever side reached 0 HP.
    void RemoveLosers_(Unit& rAttacker, Unit& rDefender, CombatResult_t& rResult);
    // Stack collateral for the kill, then the defender goes through RemoveDefeatedUnit; captures
    // are recorded on rResult.
    void RemoveDefeatedDefender_(Unit& rAttacker, Unit& rDefender, CombatResult_t& rResult);

    int Roll_(int strength) const;
    // Both rolls for a round, drawn from the strengths on rResult.
    CombatRound_t RollRound_(const CombatResult_t& rResult) const;
    CombatRound_t PlayRound_(Unit& rAttacker, Unit& rDefender, const CombatResult_t& rResult);
    CombatRound_t PlayStrikeRound_(Unit& rAttacker, Unit& rTarget, const CombatResult_t& rResult,
                                   int hpFloor);

    DisengageRules m_disengage;
    WorldMap& m_rWorldMap;
    const TileEffectsContext& m_rTileEffects;
    const MoraleCalculator& m_rMorale;
    // Non-const reference: Roll_ is const so Resolve stays const-correct for callers.
    std::mt19937& m_rRng;
};

} // namespace ac
