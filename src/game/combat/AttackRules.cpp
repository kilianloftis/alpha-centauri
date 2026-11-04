#include "game/combat/AttackRules.h"

#include "game/CurrentGameState.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/combat/CombatRules.h"
#include "game/diplomacy/DiplomaticPermissionRules.h"
#include "game/faction/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/InteractionResolve.h"
#include "game/effects/TileEffectsContext.h"
#include "game/visibility/VisibilityRules.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementIds.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/movement/MovementConstants.h"
#include "game/movement/MovementRules.h"
#include "game/movement/TransportRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"

#include <algorithm>

namespace ac
{

namespace
{

// Harbor pad, or embarked on a same-faction carrier that carries it. Co-located but
// unembarked air over a deck is not resting (same rule as IsRefuelSite).
bool DefenderIsResting_(const Unit& rDefender, const Tile& rTile, const WorldMap& rWorldMap)
{
    const FactionId_t factionId = rDefender.GetFaction().GetFactionId();
    const UnitDomain_t domain = rDefender.GetDomain();
    if (TileHarbors(rTile, domain, factionId, rWorldMap.GetTerritory()))
    {
        return true;
    }
    if (!rDefender.Transport().IsEmbarked())
    {
        return false;
    }
    const Unit* pCarrier = rDefender.Transport().GetCarrier();
    return pCarrier && UnitCarries(*pCarrier, domain, factionId);
}

bool PassesAttackUnit_(const Unit& rAttacker, const Unit& rDefender, const Tile& rTargetTile,
                       const WorldMap& rWorldMap, const InteractionGridsConfig_t& rGrids)
{
    InteractionQuery_t q;
    q.grid = InteractionGridId_t::AttackUnit;
    q.actorDomain = rAttacker.GetDomain();
    q.targetDomain = rDefender.GetDomain();
    EffectContext_t ctx;
    ctx.pAttacker = &rAttacker;
    ctx.targetTile = &rTargetTile;
    return ResolveInteractionCell(rGrids, q, &rAttacker, ctx) != InteractionCell_t::Deny
           || DefenderIsResting_(rDefender, rTargetTile, rWorldMap);
}

std::vector<std::string> NonBaseImprovementIds_(const Tile& rTile)
{
    std::vector<std::string> ids;
    for (const ImprovementConfig_t* pImprovement : rTile.GetImprovements())
    {
        if (pImprovement->id != ImprovementIds::k_Base)
        {
            ids.push_back(pImprovement->id);
        }
    }
    return ids;
}

// Land on land, sea on sea; air and orbital always match (Console_destroy #NOTINTRIAD).
bool DomainMatchesTile_(const Unit& rUnit, const Tile& rTile)
{
    switch (rUnit.GetDomain())
    {
    case UnitDomain_t::Air:
    case UnitDomain_t::Orbital:
        return true;
    case UnitDomain_t::Land:
        return rTile.IsLand();
    case UnitDomain_t::Sea:
        return rTile.IsWater();
    }
    return false;
}

// Surface hostiles, then embarked cargo when the tile has a base.
void AppendHostiles_(const Unit& rObserver, const Tile& rTile, const WorldMap& rWorldMap,
                     std::vector<Unit*>& rOut)
{
    const FactionId_t observerId = rObserver.GetFaction().GetFactionId();
    auto consider = [&](Unit* pUnit)
    {
        if (pUnit && pUnit->GetFaction().GetFactionId() != observerId)
        {
            rOut.push_back(pUnit);
        }
    };
    for (Unit* pUnit : rWorldMap.GetUnitsOnTile(rTile))
    {
        consider(pUnit);
    }
    if (rTile.HasImprovement(ImprovementIds::k_Base))
    {
        for (Unit* pUnit : rWorldMap.GetCargoOnTile(rTile))
        {
            consider(pUnit);
        }
    }
}

} // namespace

bool CanAttackTile(const Unit& rAttacker, const Tile& rTargetTile)
{
    const GameState& rGame = CurrentGameState();
    const InteractionGridsConfig_t& rGrids = rGame.GetGameData().interactionGrids;

    // No attacking a tile the unit could not enter...
    if (!CanPhysicallyEnterTile(rAttacker, rTargetTile))
    {
        return false;
    }

    // Entry is not enough: striking from this footing is its own question, so an amphibious
    // override opens the assault without opening ocean movement.
    InteractionQuery_t q;
    q.grid = InteractionGridId_t::AttackTile;
    q.actorDomain = rAttacker.GetDomain();
    q.footing = FootingFor(rAttacker);
    EffectContext_t ctx;
    ctx.pAttacker = &rAttacker;
    ctx.targetTile = &rTargetTile;
    return ResolveInteractionCell(rGrids, q, &rAttacker, ctx) == InteractionCell_t::Allow;
}

Unit* SelectDefender(const Unit& rAttacker, const std::vector<Unit*>& rCandidates)
{
    // TODO: add SMAC's best_defender scoring behind a combat config switch: HP-scaled defense
    // minus the candidate's weapon offense, x16 for combat plans and terraformers, the designated
    // defender bonus, and ties to the later unit (deviations README "Defender selection ignores
    // unit plan and weapon").
    Unit* pBest = nullptr;
    double bestChance = 0.0;
    for (Unit* pCandidate : rCandidates)
    {
        const double chance = DefenderWinChance(rAttacker, *pCandidate);
        if (pBest == nullptr || chance > bestChance)
        {
            pBest = pCandidate;
            bestChance = chance;
        }
    }
    return pBest;
}

Unit* FindVisibleHostileOnTile(const Unit& rObserver, const Tile& rTile)
{
    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();

    std::vector<Unit*> hostiles;
    AppendHostiles_(rObserver, rTile, rWorldMap, hostiles);
    std::vector<Unit*> visible;
    for (Unit* pUnit : hostiles)
    {
        if (IsUnitVisibleTo(rObserver.GetFaction(), *pUnit))
        {
            visible.push_back(pUnit);
        }
    }
    return SelectDefender(rObserver, visible);
}

Unit* FindAttackableHostileOnTile(const Unit& rAttacker, const Tile& rTargetTile)
{
    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    const InteractionGridsConfig_t& rGrids = rGame.GetGameData().interactionGrids;

    if (rAttacker.GetMoveFragmentsRemaining() <= 0)
    {
        return nullptr;
    }
    if (!AreChebyshevAdjacent(rAttacker.GetTile(), rTargetTile, rWorldMap.GetWidth()))
    {
        return nullptr;
    }
    if (!CanAttackTile(rAttacker, rTargetTile))
    {
        return nullptr;
    }
    Unit* pDefender = FindVisibleHostileOnTile(rAttacker, rTargetTile);
    if (!pDefender)
    {
        return nullptr;
    }

    if (!PassesAttackUnit_(rAttacker, *pDefender, rTargetTile, rWorldMap, rGrids))
    {
        return nullptr;
    }
    return pDefender;
}

bool JoinsSharedStack(const Unit& rMover, const Tile& rTile)
{
    const Unit* pOccupant = FindVisibleHostileOnTile(rMover, rTile);
    return pOccupant
        && MayShareTiles(rMover.GetFaction(), pOccupant->GetFaction().GetFactionId());
}

bool CanBombard(const Unit& rAttacker)
{
    return ResolveFlag(rAttacker, RuleFlagId_t::Bombard)
           && rAttacker.GetMoveFragmentsRemaining() > 0;
}

bool IsWithinBombardRange(const Unit& rAttacker, const Tile& rTargetTile)
{
    if (!CanBombard(rAttacker))
    {
        return false;
    }
    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    const int distance = ChebyshevDistance(rAttacker.GetTile(), rTargetTile, rWorldMap.GetWidth());
    const int range = ResolveStat(rAttacker, StatId_t::BombardRange);
    return distance >= 1 && distance <= range;
}

Unit* ArtilleryDuelTarget(const Unit& rAttacker, const Tile& rTargetTile)
{
    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    const InteractionGridsConfig_t& rGrids = rGame.GetGameData().interactionGrids;

    std::vector<Unit*> hostiles;
    AppendHostiles_(rAttacker, rTargetTile, rWorldMap, hostiles);

    std::vector<Unit*> duellists;
    for (Unit* pHostile : hostiles)
    {
        if (ResolveFlag(*pHostile, RuleFlagId_t::Bombard)
            && PassesAttackUnit_(rAttacker, *pHostile, rTargetTile, rWorldMap, rGrids))
        {
            duellists.push_back(pHostile);
        }
    }
    return SelectDefender(rAttacker, duellists);
}

std::vector<Unit*> BombardStrikeTargets(const Unit& rAttacker, const Tile& rTargetTile)
{
    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    const InteractionGridsConfig_t& rGrids = rGame.GetGameData().interactionGrids;

    std::vector<Unit*> hostiles;
    AppendHostiles_(rAttacker, rTargetTile, rWorldMap, hostiles);

    std::vector<Unit*> targets;
    for (Unit* pHostile : hostiles)
    {
        if (ResolveFlag(*pHostile, RuleFlagId_t::Bombard))
        {
            return {};
        }
        if (PassesAttackUnit_(rAttacker, *pHostile, rTargetTile, rWorldMap, rGrids))
        {
            targets.push_back(pHostile);
        }
    }
    return targets;
}

std::vector<std::string> BombardImprovementTargets(const Tile& rTargetTile)
{
    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    if (!rWorldMap.GetUnitsOnTile(rTargetTile).empty()
        || !rWorldMap.GetCargoOnTile(rTargetTile).empty())
    {
        return {};
    }
    return NonBaseImprovementIds_(rTargetTile);
}

std::vector<DestroyEnhancementOption_t> DestroyEnhancementOptions(const Tile& rTile)
{
    if (rTile.HasImprovement(ImprovementIds::k_Base))
    {
        return {};
    }
    std::vector<DestroyEnhancementOption_t> options;
    for (const ImprovementConfig_t* pImprovement : rTile.GetImprovements())
    {
        if (pImprovement->id == ImprovementIds::k_Base)
        {
            continue;
        }
        options.push_back({pImprovement->id, pImprovement->name});
    }
    return options;
}

std::vector<std::string> DestroyEnhancementTargets(const Tile& rTile)
{
    std::vector<std::string> ids;
    for (const DestroyEnhancementOption_t& rOption : DestroyEnhancementOptions(rTile))
    {
        ids.push_back(rOption.improvementId);
    }
    return ids;
}

bool CanDestroyEnhancement(const Unit& rUnit)
{
    if (rUnit.GetMoveFragmentsRemaining() <= 0)
    {
        return false;
    }
    if (!rUnit.IsCombatUnit() && !ResolveFlag(rUnit, RuleFlagId_t::Terraform))
    {
        return false;
    }
    const Tile& rTile = rUnit.GetTile();
    return DomainMatchesTile_(rUnit, rTile) && !DestroyEnhancementTargets(rTile).empty();
}

int DestroyEnhancementSuccessChance(const Unit& rUnit)
{
    const EffectContext_t ctx = UnitSubjectContext(&rUnit, EffectContext_t{});
    std::vector<ActiveEffect_t> effects = CollectLiveUnitEffects(rUnit).effects;
    const FactionId_t unitFaction = rUnit.GetFaction().GetFactionId();
    for (const ActiveEffect_t& rEffect :
         CurrentGameState().GetTileEffects().CollectAreaEffects(rUnit.GetTile()))
    {
        // Friendly and unattributed auras do not cut success (same gate as airdrop interdiction).
        if (!rEffect.ownerFaction.has_value() || *rEffect.ownerFaction == unitFaction)
        {
            continue;
        }
        effects.push_back(rEffect);
    }
    const double total = ResolveStatModifiers(
                             FilterByStatIdInContext(effects, StatId_t::DestroyEnhancementSuccess,
                                                     ctx),
                             100.0, &ctx)
                             .total;
    return FinalizeResolvedStat(total);
}

int DestroyEnhancementAttempts(const Unit& rUnit)
{
    const EffectContext_t ctx = UnitSubjectContext(&rUnit, EffectContext_t{});
    std::vector<ActiveEffect_t> effects = CollectLiveUnitEffects(rUnit).effects;
    const double total =
        ResolveStatModifiers(
            FilterByStatIdInContext(effects, StatId_t::DestroyEnhancementCount, ctx), 1.0, &ctx)
            .total;
    return std::max(1, FinalizeResolvedStat(total));
}

int AttackMoveFragments(const Unit& rAttacker, AttackKind_t kind)
{
    if (kind == AttackKind_t::Bombard || kind == AttackKind_t::DestroyEnhancement
        || ResolveFlag(rAttacker, RuleFlagId_t::AttackingEndsTurn))
    {
        return rAttacker.GetMoveFragmentsRemaining();
    }
    return MovementConstants_t::k_moveFragmentsPerPoint;
}

} // namespace ac
