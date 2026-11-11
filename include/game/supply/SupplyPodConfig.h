#pragma once

#include "game/effects/TriggeredEffect.h"
#include "game/units/UnitDomain.h"

#include <optional>
#include <string>
#include <vector>

namespace ac
{

// World-gen / wreckage / starting-pod placement knobs for one event group.
struct SupplyPodPlacementConfig_t
{
    // Surface filter for where this event group's pods may be stamped.
    UnitDomain_t domain = UnitDomain_t::Land;
    double globalFraction = 0.0;
    int wreckageRadius = 0;
    int wreckageExtra = 0;
    int startRadius = 0;
    int startCount = 0;
};

// Optional eligibility gates beyond the event group's placement filter.
struct SupplyPodEventFilters_t
{
    int minTurn = 0;
};

// Player-facing copy for a pod open. `script` is a TextPack entry id; `image` is optional art.
struct SupplyPodNoticeConfig_t
{
    std::string script;
    std::optional<std::string> image;
};

struct SupplyPodEventConfig_t
{
    std::string id;
    int weight = 0;
    SupplyPodEventFilters_t filters;
    SupplyPodNoticeConfig_t notice;
    std::vector<TriggeredEffectConfig_t> effects;
};

// One weighted open table, stamped onto pods at placement by `id`.
struct SupplyPodEventGroupConfig_t
{
    std::string id;
    SupplyPodPlacementConfig_t placement;
    SupplyPodNoticeConfig_t emptyNotice;
    std::vector<SupplyPodEventConfig_t> events;
};

struct SupplyPodConfig_t
{
    std::vector<SupplyPodEventGroupConfig_t> eventGroups;

    const SupplyPodEventGroupConfig_t* FindEventGroup(const std::string& rId) const;
};

} // namespace ac
