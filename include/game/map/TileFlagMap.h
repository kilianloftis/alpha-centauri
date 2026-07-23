#pragma once

#include "lib/Revision.h"
#include <cstdint>
#include <vector>

namespace ac
{

class Tile;

// Dense per-tile boolean grid sized to a WorldMap. Storage behind FactionExploredMap
// and FactionVisibleMap (each owns its own TileFlagMap).
class TileFlagMap
{
public:
    TileFlagMap() = default;

    // Size (or resize) to the world. Clears every flag.
    void Reset(int width, int height);

    // Clear every flag without changing dimensions. No-op if unsized.
    void ClearAll();

    // Set every flag. No-op if unsized. Bumps revision only when something changes.
    void SetAll();

    bool IsSized() const { return m_width > 0 && m_height > 0; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

    bool Test(int x, int y) const;
    bool Test(const Tile& rTile) const;

    void Set(int x, int y);
    void Set(const Tile& rTile);

    // OR every set flag from rOther into this map. No-op if either is unsized or sizes differ.
    void MergeFrom(const TileFlagMap& rOther);

    uint64_t GetRevision() const { return m_revision.Get(); }

private:
    size_t Index_(int x, int y) const;
    bool InBounds_(int x, int y) const;

    int m_width = 0;
    int m_height = 0;
    std::vector<uint8_t> m_flags;
    Revision m_revision;
};

} // namespace ac
