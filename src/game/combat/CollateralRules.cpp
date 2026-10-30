#include "game/combat/CollateralRules.h"

#include "game/CurrentGameState.h"
#include "game/GameState.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/Faction.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/natives/NativeRules.h"
#include "game/units/Unit.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ac
{

namespace
{

int ResolveSplash_(const Unit& rAttacker)
{
    EffectContext_t ctx;
    ctx.pUnit = &rAttacker;
    ctx.pAttacker = &rAttacker;
    return std::max(0, ResolveCombatUnitStat(
                           rAttacker, StatId_t::CollateralDamage, ctx, {}));
}

// Tile effects and the occupant's live list, same combined stack move cost uses.
// A MaxClamp that leaves 0 (Base, Bunker) skips the occupant. A geometric 0 does not.
struct Susceptibility_t
{
    double scale = 1.0;
    bool bTileImmune = false;
};

Susceptibility_t ResolveSusceptibility_(const Unit& rOccupant, const Tile& rTile)
{
    std::vector<ActiveEffect_t> effects = CollectTileEffects(rTile);
    const UnitEffects_t live = CollectLiveUnitEffects(rOccupant);
    effects.insert(effects.end(), live.effects.begin(), live.effects.end());

    EffectContext_t ctx;
    ctx.targetTile = &rTile;
    ctx.pUnit = &rOccupant;

    const StatBreakdown_t breakdown = ResolveStatModifiers(
        FilterByStatIdInContext(effects, StatId_t::CollateralSusceptibility, ctx),
        SeedFor(StatId_t::CollateralSusceptibility),
        &ctx);

    Susceptibility_t susceptibility;
    susceptibility.scale = std::max(0.0, breakdown.total);
    bool bClamped = false;
    for (const StatBreakdown_t::Contribution_t& rContribution : breakdown.contributions)
    {
        if (rContribution.op == ModifierOp_t::MaxClamp)
        {
            bClamped = true;
            break;
        }
    }
    susceptibility.bTileImmune = bClamped && susceptibility.scale == 0.0;
    return susceptibility;
}

bool IsImmune_(const Unit* pUnit, const std::vector<CollateralHit_t>& rPlan)
{
    return std::any_of(rPlan.begin(), rPlan.end(), [pUnit](const CollateralHit_t& rHit)
                       { return rHit.pOccupant == pUnit && rHit.bImmune; });
}

} // namespace

std::vector<CollateralHit_t> PlanStackCollateral(const Unit& rAttacker, const Unit& rDefender)
{
    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    const Tile& rTile = rDefender.GetTile();
    const int splash = ResolveSplash_(rAttacker);

    std::vector<CollateralHit_t> plan;
    for (Unit* pOccupant : rWorldMap.GetUnitsOnTile(rTile))
    {
        if (pOccupant == nullptr || pOccupant == &rDefender)
        {
            continue;
        }
        CollateralHit_t hit;
        hit.pOccupant = pOccupant;
        const Susceptibility_t susceptibility = ResolveSusceptibility_(*pOccupant, rTile);
        if (susceptibility.bTileImmune)
        {
            hit.bImmune = true;
        }
        else if (IsWildNative(*pOccupant))
        {
            hit.bDestroyed = true;
        }
        else
        {
            hit.damage = static_cast<int>(std::lround(splash * susceptibility.scale));
        }
        plan.push_back(hit);
    }
    return plan;
}

std::vector<Unit*> NonCombatantsLeftUnprotected(const Unit& rAttacker, const Unit& rDefender,
                                                const std::vector<CollateralHit_t>& rPlan)
{
    if (!ResolveFlag(rAttacker.GetFaction(),
                     RuleFlagId_t::NonCombatantsDestroyedWithoutCombatant))
    {
        return {};
    }

    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    std::vector<Unit*> nonCombatants;
    for (Unit* pOccupant : rWorldMap.GetUnitsOnTile(rDefender.GetTile()))
    {
        if (pOccupant == nullptr || pOccupant == &rDefender || IsImmune_(pOccupant, rPlan))
        {
            continue;
        }
        if (!ResolveFlag(*pOccupant, RuleFlagId_t::NonCombatant))
        {
            return {};
        }
        nonCombatants.push_back(pOccupant);
    }
    return nonCombatants;
}

} // namespace ac
