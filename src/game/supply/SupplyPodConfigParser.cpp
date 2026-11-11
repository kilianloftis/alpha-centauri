#include "game/supply/SupplyPodConfigParser.h"

#include "game/effects/TriggeredEffectParser.h"
#include "lib/config/JsonConfigLoader.h"

#include <unordered_set>

namespace ac
{

namespace
{

UnitDomain_t ParsePlacementDomain_(const ConfigReader& rReader)
{
    const UnitDomain_t domain = rReader.Enum<UnitDomain_t>("domain", "unit domain");
    if (domain != UnitDomain_t::Land && domain != UnitDomain_t::Sea)
    {
        rReader.FailKey("domain", "must be 'Land' or 'Sea'");
    }
    return domain;
}

SupplyPodPlacementConfig_t ParsePlacement_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"domain", "global_fraction", "wreckage_radius", "wreckage_extra",
                               "start_radius", "start_count"});
    SupplyPodPlacementConfig_t placement;
    placement.domain = ParsePlacementDomain_(rReader);
    placement.globalFraction = rReader.NumberAtLeast("global_fraction", 0.0);
    placement.wreckageRadius = rReader.IntAtLeast("wreckage_radius", 0);
    placement.wreckageExtra = rReader.IntAtLeast("wreckage_extra", 0);
    placement.startRadius = rReader.IntAtLeast("start_radius", 0);
    placement.startCount = rReader.IntAtLeast("start_count", 0);
    return placement;
}

SupplyPodEventFilters_t ParseFilters_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"min_turn"});
    SupplyPodEventFilters_t filters;
    if (rReader.Has("min_turn"))
    {
        filters.minTurn = rReader.IntAtLeast("min_turn", 0);
    }
    return filters;
}

SupplyPodNoticeConfig_t ParseNotice_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"script", "image"});
    SupplyPodNoticeConfig_t notice;
    notice.script = rReader.NonEmptyString("script");
    if (rReader.Has("image"))
    {
        notice.image = rReader.NonEmptyString("image");
    }
    return notice;
}

SupplyPodEventConfig_t ParseEvent_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"id", "weight", "filters", "notice", "effects"});
    SupplyPodEventConfig_t event;
    event.id = rReader.NonEmptyString("id");
    event.weight = rReader.IntAtLeast("weight", 0);
    if (rReader.Has("filters"))
    {
        event.filters = ParseFilters_(rReader.Object("filters"));
    }
    event.notice = ParseNotice_(rReader.Object("notice"));
    rReader.Array("effects");
    event.effects = TriggeredEffectParser::ParseTriggeredEffects(rReader.Json(), "effects",
                                                                 "supply_pod:" + event.id);
    return event;
}

SupplyPodEventGroupConfig_t ParseEventGroup_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"id", "placement", "empty_notice", "events"});
    SupplyPodEventGroupConfig_t eventGroup;
    eventGroup.id = rReader.NonEmptyString("id");
    eventGroup.placement = ParsePlacement_(rReader.Object("placement"));
    eventGroup.emptyNotice = ParseNotice_(rReader.Object("empty_notice"));

    std::unordered_set<std::string> seenIds;
    for (const ConfigReader& rEventReader : rReader.Objects("events"))
    {
        SupplyPodEventConfig_t event = ParseEvent_(rEventReader);
        if (!seenIds.insert(event.id).second)
        {
            rReader.FailKey("events", "has duplicate event id '" + event.id + "'");
        }
        eventGroup.events.push_back(std::move(event));
    }
    return eventGroup;
}

SupplyPodConfig_t ParseSupplyPods_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"event_groups"});
    rReader.NonEmptyArray("event_groups");

    SupplyPodConfig_t config;
    std::unordered_set<std::string> seenIds;
    for (const ConfigReader& rGroupReader : rReader.Objects("event_groups"))
    {
        SupplyPodEventGroupConfig_t eventGroup = ParseEventGroup_(rGroupReader);
        if (!seenIds.insert(eventGroup.id).second)
        {
            rReader.FailKey("event_groups",
                            "has duplicate event_group id '" + eventGroup.id + "'");
        }
        config.eventGroups.push_back(std::move(eventGroup));
    }
    return config;
}

} // namespace

const SupplyPodEventGroupConfig_t* SupplyPodConfig_t::FindEventGroup(const std::string& rId) const
{
    for (const SupplyPodEventGroupConfig_t& rEventGroup : eventGroups)
    {
        if (rEventGroup.id == rId)
        {
            return &rEventGroup;
        }
    }
    return nullptr;
}

SupplyPodConfig_t SupplyPodConfigParser::ParseConfig(const std::string& rConfigPath)
{
    return JsonConfigLoader::LoadObjectFile<SupplyPodConfig_t>(rConfigPath, "supply pods",
                                                               ParseSupplyPods_);
}

} // namespace ac
