#pragma once

#include "game/effects/EffectConfig.h"
#include "game/effects/TriggeredEffect.h"

#include <string>
#include <vector>

namespace ac
{

// A deterministic repeating window: active whenever
// (yearsSinceFirstPlayable - startYearOffset) mod cycleYears < durationYears.
struct WorldEventCycle_t
{
    int cycleYears = 0;
    int durationYears = 0;
    int startYearOffset = 0;
};

// One entry of config/world_events.json. `effects` are continuous and served to every faction
// while the event is active; the triggered lists fire once on each edge.
struct WorldEventConfig_t
{
    std::string id;
    std::string name;
    WorldEventCycle_t cycle;
    std::vector<EffectConfig_t> effects;
    std::vector<TriggeredEffectConfig_t> onStartEffects;
    std::vector<TriggeredEffectConfig_t> onEndEffects;
};

struct WorldEventsConfig_t
{
    std::vector<WorldEventConfig_t> events;
};

class WorldEventsConfigParser
{
public:
    // Every key is required. trigger.kind must be "Cycle", the only trigger kind.
    WorldEventsConfig_t ParseConfig(const std::string& rConfigPath);
};

bool IsWorldEventActive(const WorldEventCycle_t& rCycle, int yearsSinceFirstPlayable);

} // namespace ac
