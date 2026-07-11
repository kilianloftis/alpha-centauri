#pragma once

#include "game/map/TileFlagMap.h"

namespace ac
{

class Faction;
class FactionExploredMap;
class Tile;
class WorldMap;

// Per-faction currently-visible tiles: recomputed from units (Vision) and bases.
// Lives on Faction (not Tile). Cleared and rebuilt whenever vision sources change;
// each rebuild also marks those tiles on the faction's FactionExploredMap.
class FactionVisibleMap
{
public:
    FactionVisibleMap() = default;

    void Reset(int width, int height) { m_flags.Reset(width, height); }
    void ClearAll() { m_flags.ClearAll(); }

    bool IsSized() const { return m_flags.IsSized(); }
    int GetWidth() const { return m_flags.GetWidth(); }
    int GetHeight() const { return m_flags.GetHeight(); }

    bool IsVisible(int x, int y) const { return m_flags.Test(x, y); }
    bool IsVisible(const Tile& rTile) const { return m_flags.Test(rTile); }

    void Mark(const Tile& rTile) { m_flags.Set(rTile); }

    // Clear current visibility, then reveal from every unit and base of rFaction.
    // Newly visible tiles are also marked on rExplored. No-op if this map is unsized.
    void RebuildFromSources(const Faction& rFaction, const WorldMap& rWorldMap,
                            FactionExploredMap& rExplored);

    uint64_t GetRevision() const { return m_flags.GetRevision(); }

private:
    void RevealAround_(const Tile& rOrigin, int radius, const WorldMap& rWorldMap,
                       FactionExploredMap& rExplored);

    TileFlagMap m_flags;
};

} // namespace ac
