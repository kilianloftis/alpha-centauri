#pragma once

#include "game/map/TileFlagMap.h"

namespace ac
{

class Faction;
class FactionExploredMap;
class FactionTileMemory;
class Tile;
class WorldMap;

// Tiles the faction sees now, from units, bases and owned vision improvements; rebuilding also
// marks them explored.
class FactionVisibleMap
{
public:
    FactionVisibleMap() = default;

    void Reset(int width, int height)
    {
        m_flags.Reset(width, height);
        m_bRemoveFog = false;
    }
    void ClearAll() { m_flags.ClearAll(); }

    bool IsSized() const { return m_flags.IsSized(); }
    int GetWidth() const { return m_flags.GetWidth(); }
    int GetHeight() const { return m_flags.GetHeight(); }

    // When RemoveFog is active, every tile is treated as currently visible.
    void SetRemoveFog(bool bRemoveFog) { m_bRemoveFog = bRemoveFog; }

    bool IsVisible(int x, int y) const { return m_bRemoveFog || m_flags.Test(x, y); }
    bool IsVisible(const Tile& rTile) const { return m_bRemoveFog || m_flags.Test(rTile); }

    // Mark every tile currently visible (used with RemoveFog bypass, or one-shot fills).
    void MarkAll() { m_flags.SetAll(); }

    // Rebuilds from every source, marking new tiles explored and recording what each visible tile
    // shows. Throws if unsized. Leaves the RemoveFog bypass alone.
    void RebuildFromSources(const Faction& rFaction, const WorldMap& rWorldMap,
        FactionExploredMap& rExplored, FactionTileMemory& rMemory);

    uint64_t GetRevision() const { return m_flags.GetRevision(); }

private:
    void RevealAround_(const Tile& rOrigin, int radius, const WorldMap& rWorldMap,
        FactionExploredMap& rExplored);

    void RecordVisible_(const WorldMap& rWorldMap, FactionTileMemory& rMemory) const;

    void Mark_(const Tile& rTile) { m_flags.Set(rTile); }

    TileFlagMap m_flags;
    bool m_bRemoveFog = false;
};

} // namespace ac
