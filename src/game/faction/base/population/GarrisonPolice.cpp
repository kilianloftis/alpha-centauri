#include "game/faction/base/population/GarrisonPolice.h"

#include "game/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"

#include <algorithm>
#include <functional>
#include <vector>

namespace ac
{

namespace
{

int UnitPoliceEffectiveness_(const Unit& rUnit)
{
    return std::max(0, ResolveStat(rUnit, StatId_t::PoliceEffectiveness));
}

int ResolveMaxPolice_(const BaseManager& rBase)
{
    const int raw = FinalizeResolvedStat(
        ResolveStatModifiers(
            FilterBaseLevelByStatId(rBase.GetBaseEffects(), StatId_t::MaxPolice),
            SeedFor(StatId_t::MaxPolice))
            .total);
    return std::max(0, raw);
}

} // namespace

int ComputeGarrisonPoliceSuppression(const BaseManager& rBase)
{
    const FactionId_t factionId = rBase.GetFaction().GetFactionId();
    const WorldMap& rMap = rBase.GetFaction().GetWorldMap();

    std::vector<int> garrisonEffectiveness;
    for (Unit* pUnit : rMap.GetUnitsOnTile(rBase.GetTile()))
    {
        if (!pUnit || pUnit->GetFaction().GetFactionId() != factionId)
        {
            continue;
        }
        if (pUnit->IsEmbarked())
        {
            continue;
        }
        const int effectiveness = UnitPoliceEffectiveness_(*pUnit);
        if (effectiveness <= 0)
        {
            continue;
        }
        garrisonEffectiveness.push_back(effectiveness);
    }
    std::sort(garrisonEffectiveness.begin(), garrisonEffectiveness.end(), std::greater<>{});
    // Highest effectiveness first so max_police slots are filled by the best police units,
    // not by tile occupancy order.

    int suppression = 0;
    const int slots = ResolveMaxPolice_(rBase);
    for (int i = 0; i < slots && i < static_cast<int>(garrisonEffectiveness.size()); ++i)
    {
        suppression += garrisonEffectiveness[static_cast<size_t>(i)];
    }
    return suppression;
}

} // namespace ac
