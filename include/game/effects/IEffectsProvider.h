#pragma once

#include "game/effects/ActiveEffect.h"
#include <cstdint>

namespace ac
{

// Owns a faction-wide effect pool.
class IEffectsProvider
{
public:
    virtual ~IEffectsProvider() = default;

    // Valid until the next change to any effect source; implementations memoize.
    virtual const FactionEffects_t& GetActiveEffects() const = 0;

    // Monotonic version of the pool; changes iff the pool content changed. Consumers can
    // memoize derived state keyed on this value (see BaseEffectsCache::Get).
    virtual uint64_t GetEffectsVersion() const = 0;

    // Own contributions only, for faction-internal axes such as social ratings. Moves
    // GetEffectsVersion like the composed pool.
    virtual const FactionEffects_t& GetLocalActiveEffects() const = 0;
};

} // namespace ac
