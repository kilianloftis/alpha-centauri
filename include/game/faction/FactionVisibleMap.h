#pragma once

#include "game/map/TileFlagMap.h"

namespace ac
{

class Faction;
class FactionExploredMap;
class FactionTileMemory;
class Tile;
class WorldMap;

// Per-faction currently-visible tiles: recomputed from units (Vision), bases, and
// territory-owned vision improvements (e.g. Sensor). Lives on Faction (not Tile).
// Cleared and rebuilt whenever vision sources change; each rebuild also marks those
// tiles on the faction's FactionExploredMap.
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
    bool IsRemoveFog() const { return m_bRemoveFog; }

    bool IsVisible(int x, int y) const { return m_bRemoveFog || m_flags.Test(x, y); }
    bool IsVisible(const Tile& rTile) const { return m_bRemoveFog || m_flags.Test(rTile); }

    // Mark every tile currently visible (used with RemoveFog bypass, or one-shot fills).
    void MarkAll() { m_flags.SetAll(); }

    // Clear current visibility, then reveal from every unit, base, and owned Sensor
    // (vision improvements) of rFaction. Newly visible tiles are also marked on rExplored, and
    // every visible tile is recorded in rMemory as it now looks (tiles that were visible until
    // this call are recorded first, so memory keeps what they showed when they left sight).
    // Throws if this map is unsized — a faction always has one, so an unsized map is a wiring
    // bug rather than a reason to see everything. Does not clear the RemoveFog bypass flag.
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
