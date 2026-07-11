#pragma once

#include "lib/Revision.h"
#include <cstdint>
#include <vector>

namespace ac
{

class Faction;
class Tile;
class WorldMap;

// Per-faction fog of war: permanent explored memory plus currently-visible tiles.
// Lives on Faction (not Tile) — the same ownership pattern as WorkedTileIndex staying
// off Tile for worked state. Visibility is recomputed from the faction's units (Vision
// stat) and bases; explored is the monotonic union of every tile that has ever been
// currently visible.
class FactionVisibilityMap
{
public:
    FactionVisibilityMap() = default;

    // Size (or resize) to the world. Clears explored and currently-visible.
    void Reset(int width, int height);

    bool IsSized() const { return m_width > 0 && m_height > 0; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

    bool IsExplored(int x, int y) const;
    bool IsVisible(int x, int y) const;
    bool IsExplored(const Tile& rTile) const;
    bool IsVisible(const Tile& rTile) const;

    // Clear currently-visible, then reveal from every unit and base of rFaction.
    // Explored grows monotonically. No-op if not yet sized.
    void RebuildFromSources(const Faction& rFaction, const WorldMap& rWorldMap);

    uint64_t GetRevision() const { return m_revision.Get(); }

private:
    size_t Index_(int x, int y) const;
    bool InBounds_(int x, int y) const;
    void RevealAround_(const Tile& rOrigin, int radius, const WorldMap& rWorldMap);

    int m_width = 0;
    int m_height = 0;
    std::vector<uint8_t> m_explored;
    std::vector<uint8_t> m_visible;
    Revision m_revision;
};

} // namespace ac
