#pragma once

#include "game/effects/ActiveEffect.h"
#include <cstdint>
#include <vector>

namespace ac
{

class Faction;

// Session surface that supplies effects originating outside a faction's own local pool:
// other factions' WorldGlobal contributions, Planetary Council world laws, and governor
// FactionGlobal extras for that faction. Bound onto Faction (mirror BindWorldMap) so
// IEffectsProvider::GetActiveEffects is one composed pool for every consumer.
class IWorldEffectsSource
{
public:
    virtual ~IWorldEffectsSource() = default;

    // Extras for rFor. Must not include rFor's own WorldGlobal (already in its local pool).
    // Cross-faction harvest must read peer *local* pools only so council extras are not
    // double-counted when peers also compose.
    virtual std::vector<ActiveEffect_t> CollectWorldExtras(const Faction& rFor) const = 0;

    // Stamp that changes whenever CollectWorldExtras(rFor) could change (peer local pool
    // versions, council revision, peer set membership).
    virtual uint64_t GetWorldCompositionStamp(const Faction& rFor) const = 0;
};

} // namespace ac
