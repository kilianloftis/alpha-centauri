#include "game/world-events/WorldEventTracker.h"

namespace ac
{

WorldEventTracker::WorldEventTracker(const WorldEventsConfig_t& rConfig)
    : m_rConfig(rConfig)
    , m_active(rConfig.events.size(), false)
{
}

WorldEventTracker::Transitions_t WorldEventTracker::Advance(int yearsSinceFirstPlayable)
{
    Transitions_t transitions;
    for (std::size_t i = 0; i < m_rConfig.events.size(); ++i)
    {
        const WorldEventConfig_t& rEvent = m_rConfig.events[i];
        const bool bActive = IsWorldEventActive(rEvent.cycle, yearsSinceFirstPlayable);
        if (bActive == m_active[i])
        {
            continue;
        }
        m_active[i] = bActive;
        (bActive ? transitions.started : transitions.ended).push_back(&rEvent);
    }
    if (!transitions.started.empty() || !transitions.ended.empty())
    {
        m_revision.Bump();
    }
    return transitions;
}

void WorldEventTracker::AppendActiveEffects(std::vector<ActiveEffect_t>& rOut) const
{
    for (std::size_t i = 0; i < m_rConfig.events.size(); ++i)
    {
        if (m_active[i])
        {
            const WorldEventConfig_t& rEvent = m_rConfig.events[i];
            ac::AppendActiveEffects(rEvent.effects, nullptr, rEvent.id, rOut);
        }
    }
}

} // namespace ac
