#pragma once

#include <span>

namespace ac
{

struct ImprovementConfig_t;

// The occupant lists a tile shows: terrain configs first (general-to-specific), then
// improvements. Non-owning views into the tile or into a faction's memory of it.
struct TileOccupants_t
{
    std::span<const ImprovementConfig_t* const> terrain;
    std::span<const ImprovementConfig_t* const> improvements;

    // Same walk as Tile::ForEachOccupant: fn takes a const ImprovementConfig_t& and returns
    // true to stop; returns whether it stopped. Null entries are skipped.
    template <typename Fn>
    bool ForEach(Fn&& fn) const
    {
        for (const ImprovementConfig_t* pConfig : terrain)
        {
            if (pConfig && fn(*pConfig))
            {
                return true;
            }
        }
        for (const ImprovementConfig_t* pConfig : improvements)
        {
            if (pConfig && fn(*pConfig))
            {
                return true;
            }
        }
        return false;
    }
};

} // namespace ac
