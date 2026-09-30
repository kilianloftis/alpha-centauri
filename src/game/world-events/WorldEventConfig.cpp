#include "game/world-events/WorldEventConfig.h"

#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"
#include "lib/config/JsonConfigLoader.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace ac
{

namespace
{

void RejectUnknownKeys_(const nlohmann::json& rJson, const std::vector<std::string>& rKnown,
                        const std::string& rContext)
{
    for (const auto& [rKey, rUnused] : rJson.items())
    {
        if (std::find(rKnown.begin(), rKnown.end(), rKey) == rKnown.end())
        {
            throw std::runtime_error(rContext + ": unknown key '" + rKey + "'");
        }
    }
}

const nlohmann::json& RequireKey_(const nlohmann::json& rJson, const char* pKey,
                                  const std::string& rContext)
{
    if (!rJson.contains(pKey))
    {
        throw std::runtime_error(rContext + ": missing required '" + pKey + "'");
    }
    return rJson.at(pKey);
}

int RequireInt_(const nlohmann::json& rJson, const char* pKey, int minimum,
                const std::string& rContext)
{
    const nlohmann::json& rValue = RequireKey_(rJson, pKey, rContext);
    if (!rValue.is_number_integer() || rValue.get<int>() < minimum)
    {
        throw std::runtime_error(rContext + ": '" + pKey + "' must be an integer >= "
                                 + std::to_string(minimum));
    }
    return rValue.get<int>();
}

WorldEventCycle_t ParseTrigger_(const nlohmann::json& rJson, const std::string& rContext)
{
    const std::string ctx = rContext + " trigger";
    if (!rJson.is_object())
    {
        throw std::runtime_error(ctx + " must be an object");
    }
    RejectUnknownKeys_(rJson, {"kind", "cycle_years", "duration_years", "start_year_offset"},
                       ctx);
    const nlohmann::json& rKind = RequireKey_(rJson, "kind", ctx);
    if (!rKind.is_string() || rKind.get<std::string>() != "Cycle")
    {
        throw std::runtime_error(ctx + ": 'kind' must be \"Cycle\"");
    }
    WorldEventCycle_t cycle;
    cycle.cycleYears = RequireInt_(rJson, "cycle_years", 1, ctx);
    cycle.durationYears = RequireInt_(rJson, "duration_years", 1, ctx);
    cycle.startYearOffset = RequireInt_(rJson, "start_year_offset", 0, ctx);
    if (cycle.durationYears > cycle.cycleYears)
    {
        throw std::runtime_error(ctx + ": 'duration_years' must not exceed 'cycle_years'");
    }
    return cycle;
}

WorldEventConfig_t ParseEvent_(const nlohmann::json& rJson, const std::string& rContext)
{
    if (!rJson.is_object())
    {
        throw std::runtime_error(rContext + ": event entry must be an object");
    }
    RejectUnknownKeys_(rJson,
                       {"id", "name", "trigger", "effects", "on_start_effects", "on_end_effects"},
                       rContext + " event");
    const nlohmann::json& rId = RequireKey_(rJson, "id", rContext);
    if (!rId.is_string() || rId.get<std::string>().empty())
    {
        throw std::runtime_error(rContext + ": event 'id' must be a non-empty string");
    }
    WorldEventConfig_t event;
    event.id = rId.get<std::string>();
    const std::string ctx = rContext + " event '" + event.id + "'";
    const nlohmann::json& rName = RequireKey_(rJson, "name", ctx);
    if (!rName.is_string())
    {
        throw std::runtime_error(ctx + ": 'name' must be a string");
    }
    event.name = rName.get<std::string>();
    event.cycle = ParseTrigger_(RequireKey_(rJson, "trigger", ctx), ctx);
    for (const char* pList : {"effects", "on_start_effects", "on_end_effects"})
    {
        if (!RequireKey_(rJson, pList, ctx).is_array())
        {
            throw std::runtime_error(ctx + ": '" + pList + "' must be an array");
        }
    }
    event.effects =
        EffectConfigParser::ParseEffects(rJson, EffectSourceKind_t::WorldEvent, event.id);
    event.onStartEffects =
        TriggeredEffectParser::ParseTriggeredEffects(rJson, "on_start_effects", event.id);
    event.onEndEffects =
        TriggeredEffectParser::ParseTriggeredEffects(rJson, "on_end_effects", event.id);
    return event;
}

} // namespace

WorldEventsConfig_t WorldEventsConfigParser::ParseConfig(const std::string& rConfigPath)
{
    return JsonConfigLoader::LoadObjectFile<WorldEventsConfig_t>(
        rConfigPath, "world events", [&rConfigPath](const nlohmann::json& rJson) {
            const std::string ctx = "World events config '" + rConfigPath + "'";
            RejectUnknownKeys_(rJson, {"events"}, ctx);
            const nlohmann::json& rEvents = RequireKey_(rJson, "events", ctx);
            if (!rEvents.is_array())
            {
                throw std::runtime_error(ctx + ": 'events' must be an array");
            }
            WorldEventsConfig_t config;
            std::unordered_set<std::string> seenIds;
            for (const nlohmann::json& rEventJson : rEvents)
            {
                WorldEventConfig_t event = ParseEvent_(rEventJson, ctx);
                if (!seenIds.insert(event.id).second)
                {
                    throw std::runtime_error(ctx + ": duplicate event id '" + event.id + "'");
                }
                config.events.push_back(std::move(event));
            }
            return config;
        });
}

bool IsWorldEventActive(const WorldEventCycle_t& rCycle, int yearsSinceFirstPlayable)
{
    const int shifted = yearsSinceFirstPlayable - rCycle.startYearOffset;
    const int phase = ((shifted % rCycle.cycleYears) + rCycle.cycleYears) % rCycle.cycleYears;
    return phase < rCycle.durationYears;
}

} // namespace ac
