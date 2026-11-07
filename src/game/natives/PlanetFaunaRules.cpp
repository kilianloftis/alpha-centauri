#include "game/natives/PlanetFaunaRules.h"

#include "game/CurrentGameState.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/Faction.h"
#include "game/map/Tile.h"
#include "game/movement/MovementRules.h"
#include "game/natives/NativeUnitRegistry.h"
#include "game/natives/PlanetFaunaCalculator.h"
#include "game/research/ResearchRules.h"

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
