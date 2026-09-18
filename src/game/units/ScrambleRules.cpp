#include "game/units/ScrambleRules.h"

#include "game/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/UnitManager.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Pathfinder.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"

namespace ac
{

namespace
{

bool HasMatchingScrambleIntercept_(const Unit& rCandidate, const Unit& rAttacker)
{
    for (const ActiveEffect_t& rEffect : rCandidate.GetDesign().CollectEffects())
    {
        if (rEffect.config->scope != EffectScope_t::ThisUnit)
        {
            continue;
        }
        if (!std::get_if<ScrambleInterceptEffect_t>(&rEffect.config->effect))
        {
            continue;
        }
        if (UnitFilterSatisfied(*rEffect.config, rAttacker))
        {
            return true;
        }
    }
    return false;
}

bool BetterScrambleCandidate_(const Unit& rCandidate, const Unit& rBest)
{
    const int candidateAttack = ResolveStat(rCandidate, StatId_t::Attack);
    const int bestAttack = ResolveStat(rBest, StatId_t::Attack);
    if (candidateAttack != bestAttack)
    {
        return candidateAttack > bestAttack;
    }
    if (rCandidate.GetCurrentHp() != rBest.GetCurrentHp())
    {
        return rCandidate.GetCurrentHp() > rBest.GetCurrentHp();
    }
    return rCandidate.GetUnitId() < rBest.GetUnitId();
}

bool CanScrambleTo_(const Unit& rCandidate,
                    const Tile& rDest,
                    const WorldMap& rWorldMap,
                    const Pathfinder& rPathfinder)
{
    const int radius = ResolveStat(rCandidate, StatId_t::InterceptRadius);
    if (radius <= 0)
    {
        return false;
    }
    if (ChebyshevDistance(rCandidate.GetTile(), rDest, rWorldMap.GetWidth()) > radius)
    {
        return false;
    }
    if (&rCandidate.GetTile() == &rDest)
    {
        return false;
    }

    const Path_t path = rPathfinder.FindPath(rCandidate, rDest);
    if (!path.bReachable || path.tiles.empty())
    {
        return false;
    }
    return path.totalCostFragments <= rCandidate.GetMoveFragmentsRemaining();
}

} // namespace

Unit* FindScrambleInterceptor(const Unit& rAttacker,
                              Unit& rOriginalDefender,
                              const WorldMap& rWorldMap,
                              const TileEffectsContext& /*rTileEffects*/,
                              const Pathfinder& rPathfinder)
{
    const Tile& rDest = rOriginalDefender.GetTile();

    Unit* pBest = nullptr;
    for (Unit& rCandidate : rOriginalDefender.GetFaction().GetUnitManager().Units())
    {
        if (&rCandidate == &rOriginalDefender)
        {
            continue;
        }
        if (!HasMatchingScrambleIntercept_(rCandidate, rAttacker))
        {
            continue;
        }
        if (!CanScrambleTo_(rCandidate, rDest, rWorldMap, rPathfinder))
        {
            continue;
        }
        if (!pBest || BetterScrambleCandidate_(rCandidate, *pBest))
        {
            pBest = &rCandidate;
        }
    }
    return pBest;
}

} // namespace ac
