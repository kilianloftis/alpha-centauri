#pragma once

#include "game/supply/SupplyPodConfig.h"

#include <random>
#include <string>

namespace ac
{

class SupplyPodIndex;
class Tile;
class Unit;

// Stable id for the no-eligible-event open. Notice copy comes from the event group's
// empty_notice.
inline constexpr const char* k_SupplyPodEmptyEventId = "Empty";

bool TileHasSupplyPod(const Tile& rTile);

// Air and orbital chassis cannot open pods.
bool CanOpenSupplyPod(const Unit& rUnit);

// Event group stamped on this pod. Throws when the tile has a pod but no stamp, or the stamp
// is not a configured event group id.
const SupplyPodEventGroupConfig_t& EventGroupForPod(const SupplyPodConfig_t& rConfig,
                                                    const SupplyPodIndex& rIndex,
                                                    const Tile& rTile);

// Weighted open choice. pEvent is null for Empty (notice only); otherwise points at the
// selected event and eventId / pNotice come from that event.
struct SupplyPodEventPick_t
{
    std::string eventId;
    const SupplyPodNoticeConfig_t* pNotice = nullptr;
    const SupplyPodEventConfig_t* pEvent = nullptr;
};

SupplyPodEventPick_t PickEvent(const SupplyPodEventGroupConfig_t& rEventGroup, int turn,
                               std::mt19937& rRng);

} // namespace ac
