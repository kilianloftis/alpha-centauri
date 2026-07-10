#pragma once

#include "game/effects/ActiveEffect.h"
#include <cstdint>

namespace ac
{

// Abstract interface for anything that owns a faction-wide active effect pool.
// Consumers query effects through this rather than having them passed down from
// layers that should not know about effect collection.
class IEffectsProvider
{
public:
    virtual ~IEffectsProvider() = default;

    // The current pool. The returned reference is valid until the next mutation of any
    // effect source (building/pop/unit/policy/base-list change); do not hold it across
    // mutations. Implementations may memoize — both methods self-validate.
    virtual const FactionEffects_t& GetActiveEffects() const = 0;

    // Monotonic version of the pool; changes iff the pool content changed. Consumers can
    // memoize derived state keyed on this value (see BaseManager::BuildBaseEffects_).
    virtual uint64_t GetEffectsVersion() const = 0;
};

} // namespace ac
