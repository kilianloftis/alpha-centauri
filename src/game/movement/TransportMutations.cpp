#include "game/movement/TransportMutations.h"

#include "game/movement/MovementRules.h"
#include "game/movement/TransportRules.h"
#include "game/units/Unit.h"

#include <vector>

namespace ac
{

bool TryAttachToTransport(Unit& rPassenger, const WorldMap& rWorldMap)
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
    return true;
}

bool TryAutoAttachOnEntry(Unit& rPassenger, const WorldMap& rWorldMap,
                          const InteractionGridsConfig_t& rGrids)
{
    if (CanHoldTileWithoutCarrier(rPassenger, rPassenger.GetTile(), rWorldMap, rGrids))
    {
        return false;
    }
    return TryAttachToTransport(rPassenger, rWorldMap);
}

bool TryAutoAttachWhenMustLand(Unit& rPassenger, const WorldMap& rWorldMap)
{
    return TryAttachToTransport(rPassenger, rWorldMap);
}

bool TryUnloadTransportInPlace(Unit& rCarrier, const WorldMap& rWorldMap,
                               const InteractionGridsConfig_t& rGrids)
{
    if (!CanUnloadTransportInPlace(rCarrier, rWorldMap, rGrids))
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
