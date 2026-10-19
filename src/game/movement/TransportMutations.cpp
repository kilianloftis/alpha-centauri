#include "game/movement/TransportMutations.h"

#include "game/movement/MovementRules.h"
#include "game/movement/TransportRules.h"
#include "game/units/Unit.h"

#include <vector>

namespace ac
{

bool TryAttachToTransport(Unit& rPassenger)
{
    if (rPassenger.IsEmbarked())
    {
        return false;
    }
    Unit* pCarrier = FindBoardableTransport(rPassenger, rPassenger.GetTile());
    if (!pCarrier)
    {
        return false;
    }
    rPassenger.EmbarkInto(*pCarrier);
    return true;
}

bool TryAutoAttachOnEntry(Unit& rPassenger)
{
    if (CanHoldTileWithoutCarrier(rPassenger, rPassenger.GetTile()))
    {
        return false;
    }
    return TryAttachToTransport(rPassenger);
}

bool TryAutoAttachWhenMustLand(Unit& rPassenger)
{
    return TryAttachToTransport(rPassenger);
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
