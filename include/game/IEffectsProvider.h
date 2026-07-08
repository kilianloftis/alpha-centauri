#pragma once

#include "lib/effects/ActiveEffect.h"

namespace ac
{

// Abstract interface for anything that owns a faction-wide active effect pool.
// Consumers query effects through this rather than having them passed down from
// layers that should not know about effect collection.
class IEffectsProvider
{
public:
    virtual ~IEffectsProvider() = default;

    virtual FactionEffects_t GetActiveEffects() const = 0;
};

} // namespace ac
