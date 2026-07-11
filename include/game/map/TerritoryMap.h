#pragma once

#include "game/faction/base/BaseTypes.h"
#include "lib/Revision.h"
#include <cstdint>
#include <vector>

namespace ac
{

class Tile;
class WorldMap;
class BaseManager;

// Sentinel for an unclaimed tile. FactionIds from IdAllocator are non-negative.
inline constexpr FactionId k_NoFactionOwner = -1;

// World-scoped ownership grid: mutually exclusive faction territory derived from bases.
// Land bases claim a Euclidean disk of radius 7 (dx^2+dy^2 <= 50) of contiguous land;
// sea bases claim radius 3 (dx^2+dy^2 <= 10) contiguous sea. Contested tiles go to the
// nearest claiming base by Euclidean distance, then lower BaseId.
//
// Rebuild when a base is created, destroyed, or changes hands (GameState::RebuildTerritory
// via Faction::SetOnBaseListChanged). Queries read the last rebuilt owners.
class TerritoryMap
{
public:
    TerritoryMap() = default;

    // Size (or resize) to the world. Clears every owner.
    void Reset(int width, int height);

    bool IsSized() const { return m_width > 0 && m_height > 0; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

    // Recompute ownership from every live base.
    void Rebuild(const WorldMap& rWorldMap, const std::vector<const BaseManager*>& rBases);

    FactionId GetOwner(int x, int y) const;
    FactionId GetOwner(const Tile& rTile) const;
    bool HasOwner(int x, int y) const;
    bool HasOwner(const Tile& rTile) const;

    uint64_t GetRevision() const { return m_revision.Get(); }

private:
    size_t Index_(int x, int y) const;
    bool InBounds_(int x, int y) const;

    int m_width = 0;
    int m_height = 0;
    std::vector<FactionId> m_owners;
    Revision m_revision;
};

} // namespace ac
