#pragma once

#include "game/effects/ActiveEffect.h"
#include "game/world-events/WorldEventConfig.h"
#include "lib/Revision.h"

#include <cstdint>
#include <vector>

namespace ac
{

// Which world events are active this turn. GameState owns one once Engine creates it; the
// WorldEvents stage advances it, and GameState::CollectWorldExtras serves the active events'
// effects to every faction.
class WorldEventTracker
{
public:
    // rConfig must outlive this tracker.
    explicit WorldEventTracker(const WorldEventsConfig_t& rConfig);

    struct Transitions_t
    {
        std::vector<const WorldEventConfig_t*> started;
        std::vector<const WorldEventConfig_t*> ended;
    };

    // Re-evaluates every event for this year and returns the ones whose state flipped.
    Transitions_t Advance(int yearsSinceFirstPlayable);

    void AppendActiveEffects(std::vector<ActiveEffect_t>& rOut) const;

    // Moves whenever the active set changes.
    uint64_t GetRevision() const { return m_revision.Get(); }

private:
    const WorldEventsConfig_t& m_rConfig;
    std::vector<bool> m_active;
    Revision m_revision;
};

} // namespace ac
