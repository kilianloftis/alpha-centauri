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

    bool IsExplored(int x, int y) const { return m_flags.Test(x, y); }
    bool IsExplored(const Tile& rTile) const { return m_flags.Test(rTile); }

    void Mark(const Tile& rTile) { m_flags.Set(rTile); }

    // Mark every tile explored (e.g. orbital satellite / RemoveShroud game rule).
    void MarkAll() { m_flags.SetAll(); }

    // Union another faction's explored memory into this one (map trade).
    void MergeFrom(const FactionExploredMap& rOther) { m_flags.MergeFrom(rOther.m_flags); }

    uint64_t GetRevision() const { return m_flags.GetRevision(); }

private:
    TileFlagMap m_flags;
};

} // namespace ac
