#include "game/units/StackCollateral.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/Faction.h"
#include "game/faction/UnitManager.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/BaseConquestRules.h"
#include "game/units/PlanetPearls.h"
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

bool IsWildNative_(const Unit& rUnit)
{
    return rUnit.GetDesign().IsNativeLife()
        && IsNativeLifeFaction(rUnit.GetFaction().GetDefinition().identity.species);
}

} // namespace

void ApplyStackCollateral(WorldMap& rWorldMap, const MoraleCalculator& rMorale,
                          Unit& rAttacker, Unit& rDefender)
{
    const Tile& rTile = rDefender.GetTile();
    const int splash = ResolveSplash_(rAttacker);

    std::vector<Unit*> others;
    for (Unit* pOccupant : rWorldMap.GetUnitsOnTile(rTile))
    {
        if (pOccupant != nullptr && pOccupant != &rDefender)
        {
            others.push_back(pOccupant);
        }
    }

    std::vector<Unit*> tileImmune;
    for (Unit* pOther : others)
    {
        const Susceptibility_t susceptibility = ResolveSusceptibility_(*pOther, rTile);
        if (susceptibility.bTileImmune)
        {
            tileImmune.push_back(pOther);
            continue;
        }
        if (IsWildNative_(*pOther))
        {
            GrantPlanetPearls(rMorale, rAttacker.GetFaction(), *pOther);
            pOther->GetFaction().GetUnitManager().DestroyUnit(*pOther);
            continue;
        }
        const int hit = static_cast<int>(std::lround(splash * susceptibility.scale));
        if (hit == 0)
        {
            continue;
        }
        pOther->SetCurrentHp(pOther->GetCurrentHp() - hit);
        if (pOther->GetCurrentHp() <= 0)
        {
            pOther->GetFaction().GetUnitManager().DestroyUnit(*pOther);
        }
    }

    if (!ResolveFlag(rAttacker.GetFaction(),
                     RuleFlagId_t::NonCombatantsDestroyedWithoutCombatant))
    {
        return;
    }

    bool bCombatantRemains = false;
    std::vector<Unit*> nonCombatants;
    for (Unit* pOccupant : rWorldMap.GetUnitsOnTile(rTile))
    {
        if (pOccupant == nullptr || pOccupant == &rDefender)
        {
            continue;
        }
        if (std::find(tileImmune.begin(), tileImmune.end(), pOccupant) != tileImmune.end())
        {
            continue;
        }
        if (ResolveFlag(*pOccupant, RuleFlagId_t::NonCombatant))
        {
            nonCombatants.push_back(pOccupant);
        }
        else
        {
            bCombatantRemains = true;
        }
    }
    if (bCombatantRemains)
    {
        return;
    }
    for (Unit* pNonCombatant : nonCombatants)
    {
        pNonCombatant->GetFaction().GetUnitManager().DestroyUnit(*pNonCombatant);
    }
}

} // namespace ac
