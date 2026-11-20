#pragma once

#include "game/effects/ActiveEffect.h"
#include <cstdint>
#include <vector>

namespace ac
{

class BaseManager;
class Faction;
class Tile;

// Effects from outside a faction's own pool: peers' WorldGlobal, council laws, governor
// extras, and the facilities of the base on a tile.
class IWorldEffectsSource
{
public:
    virtual ~IWorldEffectsSource() = default;

    // The base whose centre is rTile, or nullptr.
    virtual const BaseManager* FindBaseAt(const Tile& rTile) const = 0;

    // Excludes rFor's own WorldGlobal, and reads peers' local pools so council extras are not
    // counted twice.
    virtual std::vector<ActiveEffect_t> CollectWorldExtras(const Faction& rFor) const = 0;

    // Stamp that changes whenever CollectWorldExtras(rFor) could change (peer local pool
    // versions, council revision, peer set membership).
    virtual uint64_t GetWorldCompositionStamp(const Faction& rFor) const = 0;

    // Every faction's local WorldGlobal effects, then council world laws, once each.
    // Governor FactionGlobal extras are not part of this set.
    virtual std::vector<ActiveEffect_t> CollectSessionWorldEffects() const = 0;
};

} // namespace ac
