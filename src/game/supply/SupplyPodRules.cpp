#include "game/supply/SupplyPodRules.h"

#include "game/map/ImprovementIds.h"
#include "game/map/Tile.h"
#include "game/supply/SupplyPodIndex.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"
#include "lib/WeightedPick.h"

#include <stdexcept>
#include <vector>

namespace ac
{

namespace
{

bool EventEligible_(const SupplyPodEventConfig_t& rEvent, int turn)
{
    if (rEvent.weight <= 0)
    {
        return false;
    }
    return turn >= rEvent.filters.minTurn;
}

SupplyPodEventPick_t EmptyPick_(const SupplyPodEventGroupConfig_t& rEventGroup)
{
    return SupplyPodEventPick_t{k_SupplyPodEmptyEventId, &rEventGroup.emptyNotice, nullptr};
}

} // namespace

bool TileHasSupplyPod(const Tile& rTile)
{
    return rTile.HasFeature(ImprovementIds::k_SupplyPod);
}

bool CanOpenSupplyPod(const Unit& rUnit)
{
    const UnitDomain_t domain = rUnit.GetDomain();
    return domain != UnitDomain_t::Air && domain != UnitDomain_t::Orbital;
}

const SupplyPodEventGroupConfig_t& EventGroupForPod(const SupplyPodConfig_t& rConfig,
                                                    const SupplyPodIndex& rIndex,
                                                    const Tile& rTile)
{
    const std::string* pEventGroupId = rIndex.Find(rTile);
    if (!pEventGroupId)
    {
        throw std::runtime_error("Supply pod has no stamped event_group id");
    }
    const SupplyPodEventGroupConfig_t* pEventGroup = rConfig.FindEventGroup(*pEventGroupId);
    if (!pEventGroup)
    {
        throw std::runtime_error("Supply pod stamped unknown event_group id '" + *pEventGroupId
                                 + "'");
    }
    return *pEventGroup;
}

SupplyPodEventPick_t PickEvent(const SupplyPodEventGroupConfig_t& rEventGroup, int turn,
                               std::mt19937& rRng)
{
    std::vector<const SupplyPodEventConfig_t*> eligible;
    eligible.reserve(rEventGroup.events.size());
    for (const SupplyPodEventConfig_t& rEvent : rEventGroup.events)
    {
        if (EventEligible_(rEvent, turn))
        {
            eligible.push_back(&rEvent);
        }
    }
    const auto picked = PickWeightedIndex(
        eligible.size(), [&](std::size_t i) { return eligible[i]->weight; }, rRng);
    if (!picked)
    {
        return EmptyPick_(rEventGroup);
    }
    const SupplyPodEventConfig_t& rEvent = *eligible[*picked];
    return SupplyPodEventPick_t{rEvent.id, &rEvent.notice, &rEvent};
}

} // namespace ac
