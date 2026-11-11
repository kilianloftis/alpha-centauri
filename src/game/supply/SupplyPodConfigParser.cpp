#include "game/supply/SupplyPodConfigParser.h"

#include "game/effects/TriggeredEffectParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"

#include <magic_enum.hpp>
#include <nlohmann/json.hpp>

#include <stdexcept>
#include <unordered_set>

namespace ac
{

namespace
{

const nlohmann::json& RequireKey_(const nlohmann::json& rJson, const char* pKey,
                                  const std::string& rContext)
{
    if (!rJson.contains(pKey))
    {
        throw std::runtime_error(rContext + ": missing required '" + pKey + "'");
    }
    return rJson.at(pKey);
}

double RequireNonNegativeDouble_(const nlohmann::json& rJson, const char* pKey,
                                 const std::string& rContext)
{
    const nlohmann::json& rValue = RequireKey_(rJson, pKey, rContext);
    if (!rValue.is_number())
    {
        throw std::runtime_error(rContext + ": '" + pKey + "' must be a number");
    }
    const double value = rValue.get<double>();
    if (value < 0.0)
    {
        throw std::runtime_error(rContext + ": '" + pKey + "' must be >= 0");
    }
    return value;
}

std::string RequireNonEmptyString_(const nlohmann::json& rJson, const char* pKey,
                                   const std::string& rContext)
{
    const nlohmann::json& rValue = RequireKey_(rJson, pKey, rContext);
    if (!rValue.is_string() || rValue.get<std::string>().empty())
    {
        throw std::runtime_error(rContext + ": '" + pKey + "' must be a non-empty string");
    }
    return rValue.get<std::string>();
}

UnitDomain_t ParsePlacementDomain_(const nlohmann::json& rJson, const std::string& rCtx)
{
    const std::string domainName = RequireNonEmptyString_(rJson, "domain", rCtx);
    const std::optional<UnitDomain_t> domain = magic_enum::enum_cast<UnitDomain_t>(domainName);
    if (!domain.has_value()
        || (*domain != UnitDomain_t::Land && *domain != UnitDomain_t::Sea))
    {
        throw std::runtime_error(rCtx + ": 'domain' must be 'Land' or 'Sea'");
    }
    return *domain;
}

SupplyPodPlacementConfig_t ParsePlacement_(const nlohmann::json& rJson, const std::string& rCtx)
{
    if (!rJson.is_object())
    {
        throw std::runtime_error(rCtx + ": 'placement' must be an object");
    }
    ConfigFields::RejectUnknownKeys(
        rJson,
        {"domain", "global_fraction", "wreckage_radius", "wreckage_extra", "start_radius",
         "start_count"},
        rCtx + " placement");
    SupplyPodPlacementConfig_t placement;
    placement.domain = ParsePlacementDomain_(rJson, rCtx);
    placement.globalFraction = RequireNonNegativeDouble_(rJson, "global_fraction", rCtx);
    placement.wreckageRadius = ConfigFields::RequireIntAtLeast(rJson, "wreckage_radius", 0, rCtx);
    placement.wreckageExtra = ConfigFields::RequireIntAtLeast(rJson, "wreckage_extra", 0, rCtx);
    placement.startRadius = ConfigFields::RequireIntAtLeast(rJson, "start_radius", 0, rCtx);
    placement.startCount = ConfigFields::RequireIntAtLeast(rJson, "start_count", 0, rCtx);
    return placement;
}

SupplyPodEventFilters_t ParseFilters_(const nlohmann::json& rJson, const std::string& rCtx)
{
    if (!rJson.is_object())
    {
        throw std::runtime_error(rCtx + ": 'filters' must be an object");
    }
    ConfigFields::RejectUnknownKeys(rJson, {"min_turn"}, rCtx + " filters");
    SupplyPodEventFilters_t filters;
    if (rJson.contains("min_turn"))
    {
        filters.minTurn = ConfigFields::RequireIntAtLeast(rJson, "min_turn", 0, rCtx);
    }
    return filters;
}

SupplyPodNoticeConfig_t ParseNotice_(const nlohmann::json& rJson, const std::string& rCtx)
{
    if (!rJson.is_object())
    {
        throw std::runtime_error(rCtx + ": notice must be an object");
    }
    ConfigFields::RejectUnknownKeys(rJson, {"script", "image"}, rCtx);
    SupplyPodNoticeConfig_t notice;
    notice.script = RequireNonEmptyString_(rJson, "script", rCtx);
    if (rJson.contains("image"))
    {
        notice.image = RequireNonEmptyString_(rJson, "image", rCtx);
    }
    return notice;
}

SupplyPodEventConfig_t ParseEvent_(const nlohmann::json& rJson, const std::string& rContext)
{
    if (!rJson.is_object())
    {
        throw std::runtime_error(rContext + ": event entry must be an object");
    }
    ConfigFields::RejectUnknownKeys(
        rJson, {"id", "weight", "filters", "notice", "effects"}, rContext + " event");
    SupplyPodEventConfig_t event;
    event.id = RequireNonEmptyString_(rJson, "id", rContext);
    const std::string ctx = rContext + " event '" + event.id + "'";
    event.weight = ConfigFields::RequireIntAtLeast(rJson, "weight", 0, ctx);
    if (rJson.contains("filters"))
    {
        event.filters = ParseFilters_(rJson.at("filters"), ctx);
    }
    event.notice = ParseNotice_(RequireKey_(rJson, "notice", ctx), ctx + " notice");
    if (!rJson.contains("effects"))
    {
        throw std::runtime_error(ctx + ": missing required 'effects'");
    }
    if (!rJson.at("effects").is_array())
    {
        throw std::runtime_error(ctx + ": 'effects' must be an array");
    }
    event.effects =
        TriggeredEffectParser::ParseTriggeredEffects(rJson, "effects", "supply_pod:" + event.id);
    return event;
}

SupplyPodEventGroupConfig_t ParseEventGroup_(const nlohmann::json& rJson, const std::string& rCtx)
{
    if (!rJson.is_object())
    {
        throw std::runtime_error(rCtx + ": event_group entry must be an object");
    }
    ConfigFields::RejectUnknownKeys(
        rJson, {"id", "placement", "empty_notice", "events"}, rCtx + " event_group");
    SupplyPodEventGroupConfig_t eventGroup;
    eventGroup.id = RequireNonEmptyString_(rJson, "id", rCtx);
    const std::string groupCtx = rCtx + " event_group '" + eventGroup.id + "'";
    eventGroup.placement = ParsePlacement_(RequireKey_(rJson, "placement", groupCtx), groupCtx);
    eventGroup.emptyNotice =
        ParseNotice_(RequireKey_(rJson, "empty_notice", groupCtx), groupCtx + " empty_notice");

    const nlohmann::json& rEvents = RequireKey_(rJson, "events", groupCtx);
    if (!rEvents.is_array())
    {
        throw std::runtime_error(groupCtx + ": 'events' must be an array");
    }
    std::unordered_set<std::string> seenIds;
    for (const nlohmann::json& rEventJson : rEvents)
    {
        SupplyPodEventConfig_t event = ParseEvent_(rEventJson, groupCtx);
        if (!seenIds.insert(event.id).second)
        {
            throw std::runtime_error(groupCtx + ": duplicate event id '" + event.id + "'");
        }
        eventGroup.events.push_back(std::move(event));
    }
    return eventGroup;
}

SupplyPodConfig_t ParseSupplyPods_(const nlohmann::json& rJson, const std::string& rCtx)
{
    ConfigFields::RejectUnknownKeys(rJson, {"event_groups"}, rCtx);
    const nlohmann::json& rEventGroups = RequireKey_(rJson, "event_groups", rCtx);
    if (!rEventGroups.is_array() || rEventGroups.empty())
    {
        throw std::runtime_error(rCtx + ": 'event_groups' must be a non-empty array");
    }

    SupplyPodConfig_t config;
    std::unordered_set<std::string> seenIds;
    for (std::size_t i = 0; i < rEventGroups.size(); ++i)
    {
        const std::string entryCtx = rCtx + " event_groups[" + std::to_string(i) + "]";
        SupplyPodEventGroupConfig_t eventGroup = ParseEventGroup_(rEventGroups.at(i), entryCtx);
        if (!seenIds.insert(eventGroup.id).second)
        {
            throw std::runtime_error(entryCtx + ": duplicate event_group id '" + eventGroup.id
                                     + "'");
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
    return JsonConfigLoader::LoadObjectFile<SupplyPodConfig_t>(
        rConfigPath, "supply pods", [&rConfigPath](const nlohmann::json& rJson) {
            return ParseSupplyPods_(rJson, "Supply pods config '" + rConfigPath + "'");
        });
}

} // namespace ac
