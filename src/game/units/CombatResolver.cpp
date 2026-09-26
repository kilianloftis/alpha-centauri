#include "game/units/CombatResolver.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/Faction.h"
#include "game/faction/UnitManager.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/units/CombatStrength.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/PlanetPearls.h"
#include "game/units/StackCollateral.h"
#include "game/units/StepEvaluator.h"
#include "game/effects/TileEffectsContext.h"
#include "lib/RandomRoll.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace ac
{

CombatResolver::CombatResolver(const MoveCostCalculator& rMoveCosts,
                               const StepEvaluator& rSteps,
                               WorldMap& rWorldMap,
                               const TileEffectsContext& rTileEffects,
                               const MoraleCalculator& rMorale,
                               std::mt19937& rRng)
    : m_disengage(rMoveCosts, rSteps, rWorldMap)
    , m_rWorldMap(rWorldMap)
    , m_rTileEffects(rTileEffects)
    , m_rMorale(rMorale)
    , m_rRng(rRng)
{
}

int CombatResolver::Roll_(int strength) const
{
    if (strength <= 0)
    {
        return 0;
    }
    std::uniform_int_distribution<int> dist(0, strength - 1);
    return dist(m_rRng);
}

bool CombatResolver::TryDisengage_(Unit& rCandidate, CombatSide_t side, int startHp,
                                   bool bMayDisengage, CombatResult_t& rResult)
{
    if (!bMayDisengage || rCandidate.GetCurrentHp() <= 0)
    {
        return false;
    }
    if ((startHp - rCandidate.GetCurrentHp()) * 2 < startHp)
    {
        return false;
    }

    const std::vector<const Tile*> retreats = m_disengage.CollectRetreatTiles(rCandidate);
    if (retreats.empty())
    {
        return false;
    }

    // Roll the chance the stat actually describes, and roll it *before* committing the move.
    // DisengageChance was defined, configured (Speeder chassis ships disengage_chance: 25) and
    // documented as a percent, but never read — so every eligible unit withdrew, every time.
    // The roll gates the state change rather than following it.
    //
    // Resolved with a combat context, like every other combat stat here: the context-free
    // overload drops any modifier carrying a condition, so an IsDefending or terrain-gated
    // disengage_chance would parse, validate, and then be silently ignored.
    //
    // TODO: this is called once per combat round while the unit stays past the half-HP gate,
    // so the effective withdrawal probability compounds (25% over three rounds is ~58%, not
    // 25%). docs/game-rules/unit-components.md calls it "% chance to disengage from combat",
    // which reads as once per combat — but whether SMAC rolls per round is not recorded here,
    // so the existing per-round call site is left as-is rather than guessed at.
    const EffectContext_t disengageCtx{
        &rCandidate.GetTile(),
        side == CombatSide_t::Attacker ? CombatRole_t::Attacker : CombatRole_t::Defender};
    if (!RollPercent(ResolveStat(rCandidate, StatId_t::DisengageChance, disengageCtx), m_rRng))
    {
        return false;
    }

    std::uniform_int_distribution<size_t> pick(0, retreats.size() - 1);
    const Tile* pRetreat = retreats[pick(m_rRng)];
    m_rWorldMap.GetUnitPositions().MoveUnit(rCandidate, *pRetreat);
    rResult.pRetreatTile = pRetreat;
    if (side == CombatSide_t::Attacker)
    {
        rResult.bAttackerDisengaged = true;
    }
    else
    {
        rResult.bDefenderDisengaged = true;
    }
    return true;
}

int CombatResolver::BombardHpFloor_(const Unit& rDefender) const
{
    const int hitPoints = ResolveStat(rDefender, StatId_t::HitPoints);
    const std::vector<ActiveEffect_t> effects =
        m_rTileEffects.CollectAreaEffects(rDefender.GetTile());
    const int percent = FinalizeResolvedStat(ResolveStatModifiers(
        FilterByStatId(effects, StatId_t::BombardMinHpPercent), 0.0).total);
    return static_cast<int>(std::ceil(static_cast<double>(hitPoints)
                                      * static_cast<double>(percent) / 100.0));
}

CombatResult_t CombatResolver::Resolve(Unit& rAttacker, Unit& rDefender,
                                       const CombatResolveOptions_t& rOptions)
{
    CombatResult_t result;
    result.attackerId = rAttacker.GetUnitId();
    result.defenderId = rDefender.GetUnitId();

    const bool bStrike = rOptions.engagement == CombatEngagement_t::ArtilleryStrike;
    const bool bArtillery = bStrike
                            || rOptions.engagement == CombatEngagement_t::ArtilleryDuel;

    const StatId_t defenderStat = rOptions.engagement == CombatEngagement_t::ArtilleryDuel
                                      ? StatId_t::Attack
                                      : StatId_t::Defense;
    const CombatStrength_t strength = ResolveCombatStrength(
        rAttacker, rDefender, m_rTileEffects, m_rMorale, defenderStat);
    result.attackStrength = strength.attackStrength;
    result.defenseStrength = strength.defenseStrength;
    result.bPsiCombat = strength.bPsiCombat;

    const int attackerStartHp = rAttacker.GetCurrentHp();
    const int defenderStartHp = rDefender.GetCurrentHp();
    const bool bAttackerMayDisengage =
        !bArtillery && m_disengage.CanDisengage(rAttacker, rDefender);
    const bool bDefenderMayDisengage =
        !bArtillery && m_disengage.CanDisengage(rDefender, rAttacker);
    const int hpFloor = bStrike ? BombardHpFloor_(rDefender) : 0;

    int roundsPlayed = 0;
    while (rAttacker.GetCurrentHp() > 0 && rDefender.GetCurrentHp() > 0)
    {
        if (bStrike && roundsPlayed >= 1)
        {
            break;
        }

        CombatRound_t round;
        round.attackRoll = Roll_(result.attackStrength);
        round.defenseRoll = Roll_(result.defenseStrength);
        round.damage = k_roundDamage;

        // Strict greater-than for the attacker; equal rolls favour the defender.
        if (round.attackRoll > round.defenseRoll)
        {
            round.roundWinner = CombatSide_t::Attacker;
            round.damage = result.bPsiCombat
                               ? std::max(1, ResolveStat(rDefender, StatId_t::PsiDamage))
                               : k_roundDamage;
            const int current = rDefender.GetCurrentHp();
            const int next = bStrike
                                 ? std::max(current - round.damage, std::min(current, hpFloor))
                                 : current - round.damage;
            rDefender.SetCurrentHp(next);
        }
        else
        {
            round.roundWinner = CombatSide_t::Defender;
            round.damage = result.bPsiCombat
                               ? std::max(1, ResolveStat(rAttacker, StatId_t::PsiDamage))
                               : k_roundDamage;
            if (!bStrike)
            {
                rAttacker.SetCurrentHp(rAttacker.GetCurrentHp() - round.damage);
            }
        }

        round.attackerHpAfter = rAttacker.GetCurrentHp();
        round.defenderHpAfter = rDefender.GetCurrentHp();
        result.rounds.push_back(round);
        ++roundsPlayed;

        if (bArtillery)
        {
            continue;
        }

        // The side that just took damage is checked first (it may have newly crossed the
        // half-HP threshold); then the other, in case both already qualify.
        const CombatSide_t damaged =
            round.roundWinner == CombatSide_t::Attacker ? CombatSide_t::Defender
                                                        : CombatSide_t::Attacker;
        const bool bDisengaged =
            damaged == CombatSide_t::Defender
                ? (TryDisengage_(rDefender, CombatSide_t::Defender, defenderStartHp,
                                 bDefenderMayDisengage, result)
                   || TryDisengage_(rAttacker, CombatSide_t::Attacker, attackerStartHp,
                                    bAttackerMayDisengage, result))
                : (TryDisengage_(rAttacker, CombatSide_t::Attacker, attackerStartHp,
                                 bAttackerMayDisengage, result)
                   || TryDisengage_(rDefender, CombatSide_t::Defender, defenderStartHp,
                                    bDefenderMayDisengage, result));
        if (bDisengaged)
        {
            break;
        }
    }

    result.bAttackerDestroyed =
        rAttacker.GetCurrentHp() <= 0 && !result.bAttackerDisengaged;
    result.bDefenderDestroyed =
        rDefender.GetCurrentHp() <= 0 && !result.bDefenderDisengaged;
    if (result.bAttackerDestroyed || result.bAttackerDisengaged)
    {
        result.victor = CombatSide_t::Defender;
    }
    else
    {
        result.victor = CombatSide_t::Attacker;
    }

    if (result.bDefenderDestroyed)
    {
        ApplyStackCollateral(m_rWorldMap, m_rMorale, rAttacker, rDefender);
        GrantPlanetPearls(m_rMorale, rAttacker.GetFaction(), rDefender);
        rDefender.GetFaction().GetUnitManager().DestroyUnit(rDefender);
    }
    if (result.bAttackerDestroyed)
    {
        rAttacker.GetFaction().GetUnitManager().DestroyUnit(rAttacker);
    }

    return result;
}

} // namespace ac
