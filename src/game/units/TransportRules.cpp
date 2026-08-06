#include "game/units/TransportRules.h"

#include "game/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/BonusEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/MovementRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"

#include <vector>

namespace ac
{

namespace
{

template <typename Fn>
void ForEachTransportParams_(const Unit& rCarrier, Fn&& rFn)
{
    for (const ActiveEffect_t& rEffect : CollectLiveUnitEffects(rCarrier))
    {
        const auto* pParams = std::get_if<TransportParamsEffect_t>(&rEffect.config->effect);
        if (pParams)
        {
            rFn(*pParams);
        }
    }
}

} // namespace

std::unordered_set<UnitDomain_t> ResolvePassengerDomains(const Unit& rCarrier)
{
    std::unordered_set<UnitDomain_t> domains;
    bool bSawPassengerDomains = false;
    ForEachTransportParams_(rCarrier, [&](const TransportParamsEffect_t& rParams)
    {
        if (rParams.passengerDomains.empty())
        {
            return;
        }
        bSawPassengerDomains = true;
        for (UnitDomain_t domain : rParams.passengerDomains)
        {
            domains.insert(domain);
        }
    });
    if (!bSawPassengerDomains && HasCargoCapacity(rCarrier))
    {
        domains.insert(UnitDomain_t::Land);
    }
    return domains;
}

std::unordered_set<RuleFlagId_t> ResolveLoadSiteFlags(const Unit& rCarrier)
{
    std::unordered_set<RuleFlagId_t> flags;
    ForEachTransportParams_(rCarrier, [&](const TransportParamsEffect_t& rParams)
    {
        for (RuleFlagId_t flag : rParams.loadSiteFlags)
        {
            flags.insert(flag);
        }
    });
    return flags;
}

bool HasCargoCapacity(const Unit& rCarrier)
{
    return ResolveStat(rCarrier, StatId_t::CargoCapacity) > 0;
}

int FreeCargoSlots(const Unit& rCarrier)
{
    const int capacity = ResolveStat(rCarrier, StatId_t::CargoCapacity);
    const int used = static_cast<int>(rCarrier.GetCargo().size());
    return capacity - used;
}

bool CanCarryPassenger(const Unit& rCarrier, const Unit& rPassenger)
{
    if (&rCarrier == &rPassenger)
    {
        return false;
    }
    if (rCarrier.GetFaction().GetFactionId() != rPassenger.GetFaction().GetFactionId())
    {
        return false;
    }
    if (rCarrier.IsEmbarked() || FreeCargoSlots(rCarrier) <= 0)
    {
        return false;
    }
    // Already aboard this carrier (capacity already counts them).
    if (rPassenger.GetCarrier() == &rCarrier)
    {
        return false;
    }
    const auto domains = ResolvePassengerDomains(rCarrier);
    return domains.contains(rPassenger.GetDomain());
}

bool CanLoadAtTile(const Unit& rCarrier, const Tile& rTile, const WorldMap& rWorldMap)
{
    const auto requiredFlags = ResolveLoadSiteFlags(rCarrier);
    if (requiredFlags.empty())
    {
        return true; // no capability required — load anywhere the carrier can be
    }
    const FactionId_t factionId = rCarrier.GetFaction().GetFactionId();
    for (RuleFlagId_t flag : requiredFlags)
    {
        if (TileProvidesFlag(rTile, flag, rWorldMap, factionId))
        {
            return true;
        }
    }
    return false;
}

Unit* FindBoardableTransport(const Unit& rPassenger, const Tile& rTile,
                             const WorldMap& rWorldMap)
{
    for (Unit* pUnit : rWorldMap.GetUnitsOnTile(rTile))
    {
        if (!pUnit || pUnit == &rPassenger || pUnit->IsEmbarked())
        {
            continue;
        }
        if (CanCarryPassenger(*pUnit, rPassenger) && CanLoadAtTile(*pUnit, rTile, rWorldMap))
        {
            return pUnit;
        }
    }
    return nullptr;
}

bool CanUnloadTo(const Unit& rPassenger, const Tile& rFrom, const Tile& rTo,
                 const WorldMap& rWorldMap)
{
    if (!rPassenger.IsEmbarked())
    {
        return false;
    }
    if (!AreChebyshevAdjacent(rFrom, rTo, rWorldMap.GetWidth()))
    {
        return false;
    }
    // Unload uses full enter rules (friendly base, Permission(Enter) sea base, land, etc.).
    return CanEnterTile(rPassenger, rTo, rWorldMap);
}

bool TryAttachToTransport(Unit& rPassenger, const WorldMap& rWorldMap, bool bRefuelOnAttach)
{
    if (rPassenger.IsEmbarked())
    {
        return false;
    }
    Unit* pCarrier = FindBoardableTransport(rPassenger, rPassenger.GetTile(), rWorldMap);
    if (!pCarrier)
    {
        return false;
    }
    rPassenger.EmbarkInto(*pCarrier);
    if (bRefuelOnAttach)
    {
        rPassenger.SetCurrentFuel(rPassenger.GetStat(StatId_t::Fuel));
    }
    return true;
}

bool TryAutoAttachOnEntry(Unit& rPassenger, const WorldMap& rWorldMap)
{
    if (CanOccupyTileUnaided(rPassenger, rPassenger.GetTile()))
    {
        return false;
    }
    return TryAttachToTransport(rPassenger, rWorldMap, /*bRefuelOnAttach=*/false);
}

bool TryAutoAttachWhenMustLand(Unit& rPassenger, const WorldMap& rWorldMap)
{
    return TryAttachToTransport(rPassenger, rWorldMap, /*bRefuelOnAttach=*/true);
}

bool SurvivesCarrierLoss(const Unit& rPassenger, const Tile& rTile)
{
    return CanOccupyTileUnaided(rPassenger, rTile);
}

bool CanUnloadTransportInPlace(const Unit& rCarrier)
{
    if (rCarrier.GetDomain() != UnitDomain_t::Air || rCarrier.GetCargo().empty())
    {
        return false;
    }
    const Tile& rTile = rCarrier.GetTile();
    for (const Unit* pPassenger : rCarrier.GetCargo())
    {
        // Dropping in place leaves nothing under the passenger, so it must hold the tile
        // on its own — boarding another carrier here is not what this order does.
        if (!pPassenger || !CanOccupyTileUnaided(*pPassenger, rTile))
        {
            return false;
        }
    }
    return true;
}

bool TryUnloadTransportInPlace(Unit& rCarrier)
{
    if (!CanUnloadTransportInPlace(rCarrier))
    {
        return false;
    }
    std::vector<Unit*> cargo = rCarrier.GetCargo();
    for (Unit* pPassenger : cargo)
    {
        if (pPassenger)
        {
            pPassenger->Disembark();
        }
    }
    return true;
}

} // namespace ac
