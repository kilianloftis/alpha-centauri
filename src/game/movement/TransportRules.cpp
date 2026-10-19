#include "game/movement/TransportRules.h"

#include "game/CurrentGameState.h"
#include "game/GameState.h"
#include "game/faction/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/movement/MovementRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"

namespace ac
{

namespace
{

template <typename Fn>
void ForEachTransportParams_(const Unit& rCarrier, Fn&& rFn)
{
    for (const ActiveEffect_t& rEffect : CollectLiveUnitEffects(rCarrier).effects)
    {
        const auto* pParams = std::get_if<TransportParamsEffect_t>(&rEffect.config->effect);
        if (pParams)
        {
            rFn(*pParams);
        }
    }
}

} // namespace

std::unordered_set<UnitDomain_t> ResolveCarriedDomains(const Unit& rCarrier)
{
    std::unordered_set<UnitDomain_t> domains;
    bool bSawCarries = false;
    ForEachTransportParams_(rCarrier, [&](const TransportParamsEffect_t& rParams)
    {
        if (rParams.carries.empty())
        {
            return;
        }
        bSawCarries = true;
        for (UnitDomain_t domain : rParams.carries)
        {
            domains.insert(domain);
        }
    });
    if (!bSawCarries && HasCargoCapacity(rCarrier))
    {
        domains.insert(UnitDomain_t::Land);
    }
    return domains;
}

bool UnitCarries(const Unit& rCarrier, UnitDomain_t domain, FactionId_t factionId)
{
    if (rCarrier.GetFaction().GetFactionId() != factionId)
    {
        return false;
    }
    return ResolveCarriedDomains(rCarrier).contains(domain);
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
    if (rCarrier.IsEmbarked() || FreeCargoSlots(rCarrier) <= 0)
    {
        return false;
    }
    // Already aboard this carrier (capacity already counts them).
    if (rPassenger.GetCarrier() == &rCarrier)
    {
        return false;
    }
    return UnitCarries(rCarrier, rPassenger.GetDomain(), rPassenger.GetFaction().GetFactionId());
}

bool CanLoadAtTile(const Unit& rCarrier, const Tile& rTile)
{
    bool bNeedsHarbor = false;
    ForEachTransportParams_(rCarrier, [&](const TransportParamsEffect_t& rParams)
    {
        if (rParams.requiresHarbor)
        {
            bNeedsHarbor = true;
        }
    });
    if (!bNeedsHarbor)
    {
        return true;
    }
    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    return TileHarbors(rTile, rCarrier.GetDomain(), rCarrier.GetFaction().GetFactionId(),
                       rWorldMap.GetTerritory());
}

Unit* FindBoardableTransport(const Unit& rPassenger, const Tile& rTile)
{
    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    for (Unit* pUnit : rWorldMap.GetUnitsOnTile(rTile))
    {
        if (!pUnit || pUnit == &rPassenger)
        {
            continue;
        }
        if (CanCarryPassenger(*pUnit, rPassenger) && CanLoadAtTile(*pUnit, rTile))
        {
            return pUnit;
        }
    }
    return nullptr;
}

bool CanUnloadTo(const Unit& rPassenger, const Tile& rFrom, const Tile& rTo)
{
    if (!rPassenger.IsEmbarked())
    {
        return false;
    }
    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    if (!AreChebyshevAdjacent(rFrom, rTo, rWorldMap.GetWidth()))
    {
        return false;
    }
    // Unload uses the physical enter rules (friendly harbor, InteractionOverride sea base,
    // land, etc.). Territory is the step's own check (StepEvaluator, BlockedByTerritory).
    return CanPhysicallyEnterTile(rPassenger, rTo);
}

bool SurvivesCarrierLoss(const Unit& rPassenger, const Tile& rTile)
{
    return CanHoldTileWithoutCarrier(rPassenger, rTile);
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
        if (!pPassenger || !CanHoldTileWithoutCarrier(*pPassenger, rTile))
        {
            return false;
        }
    }
    return true;
}

} // namespace ac
