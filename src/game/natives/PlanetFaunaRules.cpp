#include "game/natives/PlanetFaunaRules.h"

#include "game/CurrentGameState.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/Faction.h"
#include "game/map/ImprovementIds.h"
#include "game/map/RegionCache.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/movement/MovementRules.h"
#include "game/natives/NativeLifeConfig.h"
#include "game/natives/NativeUnitRegistry.h"
#include "game/natives/PlanetFaunaCalculator.h"
#include "game/research/ResearchRules.h"

#include <magic_enum.hpp>

#include <algorithm>
#include <stdexcept>

namespace ac
{

bool PlanetFaunaSuppressed()
{
    for (const Faction& rFaction : CurrentGameState().Factions())
    {
        if (ResolveFlag(rFaction, RuleFlagId_t::SuppressPlanetFauna))
        {
            return true;
        }
    }
    return false;
}

bool PlanetFaunaEarlyLimit(int turnNumber, const PlanetFaunaCalculator& rCalc, int faunaForms,
                           int faunaDiff)
{
    return turnNumber < rCalc.EarlyLimitThreshold(faunaForms, faunaDiff);
}

double ResolvePlanetFaunaCargoScale(const Faction& rFaction)
{
    return ResolveFactionStat(rFaction.GetActiveEffects(), StatId_t::PlanetFaunaCargoScale,
                              SeedFor(StatId_t::PlanetFaunaCargoScale), nullptr);
}

int PlanetFaunaBoatCargo(int turnNumber, const PlanetFaunaCalculator& rCalc, int faunaForms,
                         int faunaDiff, double cargoScale, bool bEarlyLimit)
{
    if (bEarlyLimit)
    {
        return 1;
    }
    return rCalc.BoatCargo(turnNumber, faunaForms, faunaDiff, cargoScale);
}

bool PlanetFaunaSwarmYearReached(int turnNumber, const PlanetFaunaCalculator& rCalc,
                                 int faunaForms)
{
    return turnNumber >= rCalc.SwarmYearThreshold(faunaForms);
}

bool AcceptsBoatSpawnSite(const Tile& rTile, const WorldMap& rMap)
{
    if (!rTile.IsWater())
    {
        return false;
    }
    // Sea fungus only (deviation: SMAC is_fungus at 0x5226b3 also accepts land). Shelf gate
    // matches the binary: BIT_FUNGUS and alt_level >= ALT_OCEAN_SHELF, not deep ocean.
    if (rTile.HasFeature(ImprovementIds::k_Fungus))
    {
        const auto deepOceanId = magic_enum::enum_name(TerrainFeature_t::Ocean);
        return !rTile.HasFeature(deepOceanId);
    }
    if (!rMap.GetUnitsOnTile(rTile).empty())
    {
        return false;
    }
    const NativeLifeConfig_t* pLife = CurrentGameState().GetGameData().nativeLifeConfig.get();
    if (!pLife)
    {
        throw std::logic_error("AcceptsBoatSpawnSite: no native life config is available");
    }
    const int regionTiles = rMap.GetRegions().TileCountInRegion(rTile);
    return regionTiles >= pLife->boatEmptyOceanMinRegionTiles;
}

std::vector<const NativeUnitConfig_t*> FaunaCandidatesForRole(NativeFaunaRole_t role,
                                                              const Tile* pDomainTile)
{
    std::vector<const NativeUnitConfig_t*> candidates;
    const NativeUnitRegistry* pNatives =
        CurrentGameState().GetGameData().nativeUnitRegistry.get();
    if (!pNatives)
    {
        throw std::logic_error("FaunaCandidatesForRole: no native unit registry is available");
    }
    for (const NativeUnitConfig_t& rConfig : pNatives->GetAll())
    {
        if (!rConfig.planet.has_value())
        {
            continue;
        }
        const NativeUnitPlanetConfig_t& rPlanet = *rConfig.planet;
        const bool bHasRole =
            std::find(rPlanet.faunaRoles.begin(), rPlanet.faunaRoles.end(), role)
            != rPlanet.faunaRoles.end();
        if (!bHasRole)
        {
            continue;
        }
        if (pDomainTile)
        {
            const bool bFits = CanEnterTileTerrain(rConfig.domain, *pDomainTile);
            if (!bFits)
            {
                continue;
            }
        }
        if (!AnyFactionHasTech(rPlanet.requiresAnyTech))
        {
            continue;
        }
        candidates.push_back(&rConfig);
    }
    return candidates;
}

} // namespace ac
