#include "game/faction/base/population/AwayFromHomeDrones.h"

#include "game/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/HomeBaseIndex.h"
#include "game/map/TerritoryMap.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"

#include <cmath>
#include <vector>

namespace ac
{

namespace
{

double UnitAwayWeight_(const Unit& rUnit)
{
    const std::vector<ActiveEffect_t> effects = CollectLiveUnitEffects(rUnit);
    return ResolveStatModifiersTotal(
        FilterByStatId(effects, StatId_t::AwayFromHomeDrones),
        SeedFor(StatId_t::AwayFromHomeDrones));
}

} // namespace

int ComputeAwayFromHomeDrones(const BaseManager& rBase)
{
    const FactionId_t factionId = rBase.GetFaction().GetFactionId();
    const TerritoryMap& rTerritory = rBase.GetFaction().GetWorldMap().GetTerritory();

    double weightSum = 0.0;
    for (const Unit* pUnit : rBase.GetHomeUnits().GetUnits())
    {
        if (!pUnit)
        {
            continue;
        }
        const double weight = UnitAwayWeight_(*pUnit);
        if (weight <= 0.0)
        {
            continue;
        }
        if (rTerritory.GetOwner(pUnit->GetTile()) == factionId)
        {
            continue;
        }
        weightSum += weight;
    }

    const double resolved = ResolveStatModifiers(
                                FilterBaseLevelByStatId(rBase.GetBaseEffects(),
                                                        StatId_t::AwayFromHomeDrones),
                                weightSum)
                                .total;
    return static_cast<int>(std::floor(std::max(0.0, resolved)));
}

} // namespace ac
