#include "ui/TileAutotile.h"

#include "game/map/MapUtils.h"

#include <array>
#include <stdexcept>

namespace ac
{

namespace
{

// Blob layout's eight neighbor bits start at NW; k_RingNeighbors starts at N (link cell order).
constexpr std::array<std::size_t, 8> k_BlobRingIndex = {7, 0, 1, 2, 3, 4, 5, 6};

constexpr std::uint8_t k_BlobEdgeBits = 0b10101010;

std::uint8_t MatchEdgeMask_(const Tile& rTile, const WorldMap& rMap,
                            const std::function<bool(const Tile& rNeighbor)>& matches)
{
    std::uint8_t mask = 0;
    for (std::size_t bit = 0; bit < k_EdgeNeighbors.size(); ++bit)
    {
        const LatticeOffset_t& offset = k_EdgeNeighbors[bit];
        const Tile* pNeighbor = GetTileAtLatticeOffset(rMap, rTile, offset.p, offset.q);
        if (pNeighbor && matches(*pNeighbor))
        {
            mask = static_cast<std::uint8_t>(mask | (1u << bit));
        }
    }
    return mask;
}

std::uint8_t MatchBlobMask_(const Tile& rTile, const WorldMap& rMap,
                            const std::function<bool(const Tile& rNeighbor)>& matches)
{
    std::uint8_t mask = 0;
    for (std::size_t bit = 0; bit < k_BlobRingIndex.size(); ++bit)
    {
        const LatticeOffset_t& offset = k_RingNeighbors[k_BlobRingIndex[bit]];
        const Tile* pNeighbor = GetTileAtLatticeOffset(rMap, rTile, offset.p, offset.q);
        if (pNeighbor && matches(*pNeighbor))
        {
            mask = static_cast<std::uint8_t>(mask | (1u << bit));
        }
    }
    return mask;
}

std::uint8_t ReduceBlobMask_(std::uint8_t mask)
{
    std::uint8_t reduced = mask & k_BlobEdgeBits;
    for (unsigned corner = 0; corner < 8; corner += 2)
    {
        const unsigned before = (corner + 7) % 8;
        const unsigned after = corner + 1;
        if ((mask >> corner & 1u) && (mask >> before & 1u) && (mask >> after & 1u))
        {
            reduced = static_cast<std::uint8_t>(reduced | (1u << corner));
        }
    }
    return reduced;
}

} // namespace

std::uint8_t ResolveTileMask(SpriteTileLayout_t layout, const Tile& rTile, const WorldMap& rMap,
                             const std::function<bool(const Tile& rNeighbor)>& matches)
{
    switch (layout)
    {
        case SpriteTileLayout_t::Edges:
            return MatchEdgeMask_(rTile, rMap, matches);
        case SpriteTileLayout_t::Blob:
            return ReduceBlobMask_(MatchBlobMask_(rTile, rMap, matches));
    }
    throw std::invalid_argument("ResolveTileMask: unhandled SpriteTileLayout_t");
}

} // namespace ac
