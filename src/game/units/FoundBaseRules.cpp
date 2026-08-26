#include "game/units/FoundBaseRules.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/map/MapUtils.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"

#include <algorithm>
#include <vector>

namespace ac
{

namespace
{

std::vector<const BaseManager*> CollectAllBases_(const GameState& rGameState)
{
    std::vector<const BaseManager*> bases;
    for (const Faction& rFaction : rGameState.Factions())
    {
        for (const BaseManager& rBase : rFaction.Bases())
        {
            bases.push_back(&rBase);
        }
    }
    return bases;
}

} // namespace

bool IsTooCloseToAnyBase(const Tile& rTile, const WorldMap& rWorldMap,
                         const std::vector<const BaseManager*>& rBases)
{
    const int mapWidth = rWorldMap.GetWidth();
    for (const BaseManager* pBase : rBases)
    {
        if (!pBase)
        {
            continue;
        }
        if (ChebyshevDistance(rTile, pBase->GetTile(), mapWidth) < k_MinBaseFoundingSeparation)
        {
            return true;
        }
    }
    return false;
}

bool IsInForeignTerritory(const Tile& rTile, FactionId_t founderFactionId,
                          const TerritoryMap& rTerritory)
{
    const FactionId_t owner = rTerritory.GetOwner(rTile);
    return owner != k_NoFactionOwner && owner != founderFactionId;
}

bool CanFoundBaseAt(const Tile& rTile, FactionId_t founderFactionId, const WorldMap& rWorldMap,
                    const std::vector<const BaseManager*>& rBases)
{
    if (IsTooCloseToAnyBase(rTile, rWorldMap, rBases))
    {
        return false;
    }
    if (IsInForeignTerritory(rTile, founderFactionId, rWorldMap.GetTerritory()))
    {
        return false;
    }
    return true;
}

bool CanFoundBaseAt(const Tile& rTile, FactionId_t founderFactionId, const GameState& rGameState)
{
    return CanFoundBaseAt(rTile, founderFactionId, rGameState.GetWorldMap(),
                          CollectAllBases_(rGameState));
}

int ResolveStartingMinerals(const BaseManager& rBase, const Unit* pFoundingUnit)
{
    std::vector<ActiveEffect_t> combined;
    const BaseEffects_t& rBaseEffects = rBase.GetBaseEffects();
    // The same ctx filters and resolves: the base lane may carry a BaseSize amount_source, and
    // admitting one on a subject the resolve step doesn't get is how it throws instead of scaling.
    const EffectContext_t ctx{.pBase = &rBase};
    for (const ActiveEffect_t& rEffect :
         FilterBaseLevelByStatId(rBaseEffects, StatId_t::StartingMinerals, &ctx))
    {
        combined.push_back(rEffect);
    }

    if (pFoundingUnit)
    {
        // Materialize: FilterByStatId borrows the vector. Context-free on purpose — the unit
        // lane carries ThisUnit / FactionUnits scopes, which no amount_source is legal on.
        const std::vector<ActiveEffect_t> unitEffects = CollectLiveUnitEffects(*pFoundingUnit).effects;
        for (const ActiveEffect_t& rEffect :
             FilterByStatId(unitEffects, StatId_t::StartingMinerals))
        {
            combined.push_back(rEffect);
        }
    }

    const int raw = FinalizeResolvedStat(
        ResolveStatModifiers(combined, SeedFor(StatId_t::StartingMinerals), &ctx).total);
    return std::max(0, raw);
}

void ApplyStartingMinerals(BaseManager& rBase, const Unit* pFoundingUnit)
{
    // Founding-only: call once from TryFoundBase after CreateBase. Transfers keep the
    // existing ProductionManager (RebindFaction); snapshot restore uses SetMineralStockpile.
    // Retool stays free while turn original is still null (see ProductionManager).
    rBase.GetProduction().SetMineralStockpile(
        ResolveStartingMinerals(rBase, pFoundingUnit));
}

} // namespace ac
