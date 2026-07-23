#pragma once

#include "game/map/TileFlagMap.h"

namespace ac
{

class Tile;

// Per-faction permanent fog-of-war memory: tiles this faction has ever seen.
// Lives on Faction (not Tile). Grows monotonically as vision reveals tiles;
// never cleared except on Reset (new game / resize).
class FactionExploredMap
{
public:
    FactionExploredMap() = default;

    void Reset(int width, int height) { m_flags.Reset(width, height); }

    bool IsSized() const { return m_flags.IsSized(); }
    int GetWidth() const { return m_flags.GetWidth(); }
    int GetHeight() const { return m_flags.GetHeight(); }

    bool IsExplored(int x, int y) const
    {
#ifdef AC_DEBUG_REVEAL_MAP
        (void)x;
        (void)y;
        return true;
#else
        return m_flags.Test(x, y);
#endif
    }
    bool IsExplored(const Tile& rTile) const
    {
#ifdef AC_DEBUG_REVEAL_MAP
        (void)rTile;
        return true;
#else
        return m_flags.Test(rTile);
#endif
    }

    void Mark(const Tile& rTile) { m_flags.Set(rTile); }

    // Union another faction's explored memory into this one (map trade).
    void MergeFrom(const FactionExploredMap& rOther) { m_flags.MergeFrom(rOther.m_flags); }

    uint64_t GetRevision() const { return m_flags.GetRevision(); }

private:
    TileFlagMap m_flags;
};

} // namespace ac
