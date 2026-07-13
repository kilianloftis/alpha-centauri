#include "game/units/MovementRules.h"

#include "game/Faction.h"
#include "game/effects/EffectEnums.h"
#include "game/map/Tile.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentConfig.h"

namespace ac
{

bool UnitExertsZocOn(const Unit& rProjector, const Unit& rSubject)
{
    if (rProjector.GetFaction().GetFactionId() == rSubject.GetFaction().GetFactionId())
    {
        return false;
    }
    // Air subjects are already excluded by the domain match below; this flag is for
    // land/sea units (e.g. probes) that ignore ZOC without changing domain.
    if (ResolveFlag(rSubject, RuleFlagId_t::IgnoreZoneOfControl))
    {
        return false;
    }

    switch (rProjector.GetDomain())
    {
    case UnitDomain_t::Air:
        // Air exerts on land and sea units, not on other air units.
        return rSubject.GetDomain() != UnitDomain_t::Air;
    case UnitDomain_t::Sea:
        return rSubject.GetDomain() == UnitDomain_t::Sea;
    case UnitDomain_t::Land:
        return rSubject.GetDomain() == UnitDomain_t::Land;
    }
    return false;
}

bool CanEnterTileTerrain(const Unit& rMover, const Tile& rTile)
{
    switch (rMover.GetDomain())
    {
    case UnitDomain_t::Air:
        return true;
    case UnitDomain_t::Sea:
        return rTile.IsWater();
    case UnitDomain_t::Land:
        return rTile.IsLand();
    }
    return false;
}

} // namespace ac
