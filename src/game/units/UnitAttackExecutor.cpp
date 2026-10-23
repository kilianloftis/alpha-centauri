#include "game/units/UnitAttackExecutor.h"

#include "game/combat/InterceptMutations.h"
#include "game/combat/MoraleMutations.h"
#include "game/combat/MoraleRules.h"
#include "game/combat/ScrambleRules.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/Faction.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/units/IUnitOrderWorld.h"
#include "game/units/UnitManager.h"
#include "game/units/UnitOrderExecutor.h"

#include <algorithm>
#include <random>

namespace ac
{

UnitAttackExecutor::UnitAttackExecutor(const MoveCostRules& rMoveCosts,
                                       const StepEvaluator& rSteps, WorldMap& rWorldMap,
                                       TileEffectsContext& rTileEffects,
                                       const MoraleCalculator& rMorale, std::mt19937& rRng,
                                       IUnitOrderWorld& rWorld, UnitOrderExecutor& rOrders)
    : m_rWorldMap(rWorldMap)
    , m_rTileEffects(rTileEffects)
    , m_rMorale(rMorale)
    , m_rRng(rRng)
    , m_combat(rMoveCosts, rSteps, rWorldMap, rTileEffects, rMorale, rRng)
    , m_rWorld(rWorld)
    , m_rOrders(rOrders)
{
}

void UnitAttackExecutor::PromoteOnKill_(Unit& rAttacker, Unit& rDefender,
                                       const CombatResult_t& rResult)
{
    const std::optional<CombatSide_t> candidate = PromotionCandidate(rResult);
    if (!candidate)
    {
        return;
    }
    Unit& rSurvivor = *candidate == CombatSide_t::Attacker ? rAttacker : rDefender;
    TryPromote(rSurvivor, rResult.attackStrength, rResult.defenseStrength, m_rMorale, m_rRng);
}

bool UnitAttackExecutor::SpendAttackAction_(Unit& rAttacker, AttackKind_t kind)
{
    rAttacker.Latches().MarkAttacked();
    const int fragments = AttackMoveFragments(rAttacker, kind);
    rAttacker.SpendMoveFragments(fragments);
    rAttacker.ClearOrder();
    if (ResolveFlag(rAttacker, RuleFlagId_t::SingleUse))
    {
        rAttacker.GetFaction().GetUnitManager().DestroyUnit(rAttacker);
        return true;
    }
    return false;
}

bool UnitAttackExecutor::ApplyLastDefenderConquest_(Unit& rAttacker, const Tile& rDefenderTile)
{
    return m_rWorld.ResolvePostCombatBaseConquest(rAttacker, rDefenderTile, m_rRng)
        .bActorDestroyed;
}

CombatResult_t UnitAttackExecutor::ResolveBombardExchange_(Unit& rAttacker, Unit& rDefender,
                                                         CombatEngagement_t engagement)
{
    CombatResolveOptions_t options;
    options.engagement = engagement;
    CombatResult_t combat = m_combat.Resolve(rAttacker, rDefender, options);
    combat.bBombardPlayback = true;
    PromoteOnKill_(rAttacker, rDefender, combat);
    if (!combat.bDefenderDestroyed)
    {
        // TODO: a bombarded unit must not heal this turn, once per-turn healing exists.
        rDefender.ClearOrder();
    }
    return combat;
}

std::optional<CombatResult_t> UnitAttackExecutor::TryAttack(Unit& rAttacker,
                                                           const Tile& rTargetTile)
{
    Unit* pDefender = FindAttackableHostileOnTile(rAttacker, rTargetTile);
    if (!pDefender)
    {
        return std::nullopt;
    }

    // Reported once the attack resolves, so a sneak attack's shared-tile evacuation cannot
    // move either side before the blow lands.
    const HostileAct_t act = HostileActAgainst(rAttacker, *pDefender);
    std::optional<CombatResult_t> result = TryIntercept_(rAttacker, *pDefender);
    if (!result)
    {
        result = FightDefender_(rAttacker, *pDefender);
    }
    ReportHostileAct_(act);
    return result;
}

CombatResult_t UnitAttackExecutor::FightDefender_(Unit& rAttacker, Unit& rDefender)
{
    // Read before Resolve may destroy the defender.
    const Tile& rDefenderTile = rDefender.GetTile();

    std::vector<const Tile*> scrambleHops;
    Unit& rCombatDefender = ResolveScrambleDefender_(rAttacker, rDefender, scrambleHops);

    CombatResult_t result = m_combat.Resolve(rAttacker, rCombatDefender);
    result.scramblePath = std::move(scrambleHops);
    PromoteOnKill_(rAttacker, rCombatDefender, result);

    // Combat never moves either side, so capturing an emptied base is a later step onto it.
    if (!result.bAttackerDestroyed && SpendAttackAction_(rAttacker, AttackKind_t::Attack))
    {
        result.bAttackerDestroyed = true;
    }

    // A native raider spends itself on the raid, so playback must not show it surviving.
    if (result.bDefenderDestroyed && !result.bAttackerDestroyed
        && ApplyLastDefenderConquest_(rAttacker, rDefenderTile))
    {
        result.bAttackerDestroyed = true;
    }
    return result;
}

void UnitAttackExecutor::ReportHostileAct_(const HostileAct_t& rAct)
{
    m_rWorld.OnHostileAct(rAct);
}

std::optional<CombatResult_t> UnitAttackExecutor::TryIntercept_(Unit& rAttacker, Unit& rDefender)
{
    std::optional<CombatResult_t> intercepted =
        m_rWorld.TryInterceptAttack(rAttacker, rDefender, m_rRng);
    if (!intercepted)
    {
        return std::nullopt;
    }
    // A defender lost to its own intercept was never fought: the attack is spent, with no kill
    // to promote and no last defender lost in battle.
    if (!intercepted->bAttackerDestroyed && SpendAttackAction_(rAttacker, AttackKind_t::Attack))
    {
        intercepted->bAttackerDestroyed = true;
    }
    return intercepted;
}

std::optional<UnitAttackExecutor::BombardResult_t> UnitAttackExecutor::TryBombard(
    Unit& rAttacker, const Tile& rTargetTile)
{
    if (!IsWithinBombardRange(rAttacker, rTargetTile))
    {
        return std::nullopt;
    }

    Unit* pDuelTarget = ArtilleryDuelTarget(rAttacker, rTargetTile);
    const std::vector<Unit*> strikeTargets = BombardStrikeTargets(rAttacker, rTargetTile);
    const std::vector<std::string> improvementTargets = BombardImprovementTargets(rTargetTile);
    const std::vector<HostileAct_t> acts = BombardHostileActs(rAttacker, rTargetTile);

    BombardResult_t result;
    result.combats = FireBombard_(rAttacker, pDuelTarget, strikeTargets);
    result.bAttackerDestroyed = !result.combats.empty() && result.combats.back().bAttackerDestroyed;
    if (!improvementTargets.empty())
    {
        result.destroyedImprovementId = DestroyRandomImprovement_(rTargetTile, improvementTargets);
    }

    if (!result.bAttackerDestroyed && SpendAttackAction_(rAttacker, AttackKind_t::Bombard))
    {
        result.bAttackerDestroyed = true;
    }

    const bool bAnyDefenderDestroyed = std::ranges::any_of(
        result.combats, [](const CombatResult_t& rCombat) { return rCombat.bDefenderDestroyed; });
    if (bAnyDefenderDestroyed && !result.bAttackerDestroyed
        && ApplyLastDefenderConquest_(rAttacker, rTargetTile))
    {
        result.bAttackerDestroyed = true;
        result.combats.back().bAttackerDestroyed = true;
    }
    for (const HostileAct_t& rAct : acts)
    {
        ReportHostileAct_(rAct);
    }
    return result;
}

std::vector<CombatResult_t> UnitAttackExecutor::FireBombard_(
    Unit& rAttacker, Unit* pDuelTarget, const std::vector<Unit*>& rStrikeTargets)
{
    std::vector<CombatResult_t> combats;
    if (pDuelTarget)
    {
        combats.push_back(ResolveBombardExchange_(
            rAttacker, *pDuelTarget, CombatEngagement_t::ArtilleryDuel));
    }
    for (Unit* pTarget : rStrikeTargets)
    {
        if (!combats.empty() && combats.back().bAttackerDestroyed)
        {
            break;
        }
        combats.push_back(ResolveBombardExchange_(
            rAttacker, *pTarget, CombatEngagement_t::ArtilleryStrike));
    }
    return combats;
}

std::string UnitAttackExecutor::DestroyRandomImprovement_(
    const Tile& rTile, const std::vector<std::string>& rCandidates)
{
    std::uniform_int_distribution<size_t> pick(0, rCandidates.size() - 1);
    const std::string id = rCandidates[pick(m_rRng)];
    m_rTileEffects.RemoveOccupantWithEffects(m_rWorldMap.MutableTile(rTile), id);
    return id;
}

Unit& UnitAttackExecutor::ResolveScrambleDefender_(Unit& rAttacker,
                                                  Unit& rOriginalDefender,
                                                  std::vector<const Tile*>& rOutPath)
{
    rOutPath.clear();
    Unit* pScrambler = FindScrambler(rAttacker, rOriginalDefender);
    if (!pScrambler)
    {
        return rOriginalDefender;
    }
    if (!ScrambleTo_(*pScrambler, rOriginalDefender.GetTile(), rOutPath))
    {
        rOutPath.clear();
        return rOriginalDefender;
    }
    return *pScrambler;
}

bool UnitAttackExecutor::ScrambleTo_(Unit& rScrambler, const Tile& rDest,
                                    std::vector<const Tile*>& rOutPath)
{
    rScrambler.SetOrder(MoveOrder_t{.pDestination = &rDest});
    auto movedConn = m_rWorldMap.GetUnitPositions().OnUnitMoved.ConnectScoped(
        [&](Unit& rUnit)
        {
            if (&rUnit == &rScrambler)
            {
                rOutPath.push_back(&rUnit.GetTile());
            }
        });

    if (m_rOrders.Execute(rScrambler) == OrderProgress_t::UnitDestroyed)
    {
        return false;
    }
    if (&rScrambler.GetTile() != &rDest)
    {
        rScrambler.ClearOrder();
        return false;
    }
    return true;
}

} // namespace ac
