#include "game/world-events/WorldEventConfig.h"

#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"
#include "lib/config/JsonConfigLoader.h"

#include <unordered_set>

namespace ac
{

namespace
{

WorldEventCycle_t ParseTrigger_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"kind", "cycle_years", "duration_years", "start_year_offset"});
    if (rReader.String("kind") != "Cycle")
    {
        rReader.FailKey("kind", "must be \"Cycle\"");
    }
    WorldEventCycle_t cycle;
    cycle.cycleYears = rReader.IntAtLeast("cycle_years", 1);
    cycle.durationYears = rReader.IntAtLeast("duration_years", 1);
    cycle.startYearOffset = rReader.IntAtLeast("start_year_offset", 0);
    if (cycle.durationYears > cycle.cycleYears)
    {
        rReader.FailKey("duration_years", "must not exceed 'cycle_years'");
    }
    return cycle;
}

WorldEventConfig_t ParseEvent_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys(
        {"id", "name", "trigger", "effects", "on_start_effects", "on_end_effects"});
    WorldEventConfig_t event;
    event.id = rReader.NonEmptyString("id");
    event.name = rReader.String("name");
    event.cycle = ParseTrigger_(rReader.Object("trigger"));
    for (const char* pList : {"effects", "on_start_effects", "on_end_effects"})
    {
        rReader.Array(pList);
    }
    event.effects = EffectConfigParser::ParseEffects(rReader.Json(), EffectSourceKind_t::WorldEvent,
                                                     event.id);
    event.onStartEffects =
        TriggeredEffectParser::ParseTriggeredEffects(rReader.Json(), "on_start_effects", event.id);
    event.onEndEffects =
        TriggeredEffectParser::ParseTriggeredEffects(rReader.Json(), "on_end_effects", event.id);
    return event;
}

WorldEventsConfig_t ParseWorldEvents_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"events"});
    WorldEventsConfig_t config;
    std::unordered_set<std::string> seenIds;
    for (const ConfigReader& rEventReader : rReader.Objects("events"))
    {
        WorldEventConfig_t event = ParseEvent_(rEventReader);
        if (!seenIds.insert(event.id).second)
        {
            rReader.FailKey("events", "has duplicate event id '" + event.id + "'");
        }
        config.events.push_back(std::move(event));
    }
    return config;
}

} // namespace

WorldEventsConfig_t WorldEventsConfigParser::ParseConfig(const std::string& rConfigPath)
{
    return JsonConfigLoader::LoadObjectFile<WorldEventsConfig_t>(rConfigPath, "world events",
                                                                 ParseWorldEvents_);
}

bool IsWorldEventActive(const WorldEventCycle_t& rCycle, int yearsSinceFirstPlayable)
{
    const int shifted = yearsSinceFirstPlayable - rCycle.startYearOffset;
    const int phase = ((shifted % rCycle.cycleYears) + rCycle.cycleYears) % rCycle.cycleYears;
    return phase < rCycle.durationYears;
}

} // namespace ac
