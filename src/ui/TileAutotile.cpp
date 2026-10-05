#include "ui/TileAutotile.h"

#include <array>
#include <stdexcept>

namespace ac
{

namespace
{

struct GridDelta_t
{
    int dx = 0;
    int dy = 0;
};

constexpr std::array<GridDelta_t, 4> k_EdgeNeighbors = {{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};

constexpr std::array<GridDelta_t, 8> k_BlobNeighbors = {{
    {-1, -1}, {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0},
}};

constexpr std::uint8_t k_BlobEdgeBits = 0b10101010;

template<std::size_t N>
std::uint8_t MatchMask_(const std::array<GridDelta_t, N>& deltas, const Tile& rTile,
                        const WorldMap& rMap,
                        const std::function<bool(const Tile& rNeighbor)>& matches)
{
    std::uint8_t mask = 0;
    for (std::size_t bit = 0; bit < N; ++bit)
    {
        const Tile* pNeighbor =
            rMap.GetTile(rTile.GetX() + deltas[bit].dx, rTile.GetY() + deltas[bit].dy);
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
            return MatchMask_(k_EdgeNeighbors, rTile, rMap, matches);
        case SpriteTileLayout_t::Blob:
            return ReduceBlobMask_(MatchMask_(k_BlobNeighbors, rTile, rMap, matches));
    }
    throw std::invalid_argument("ResolveTileMask: unhandled SpriteTileLayout_t");
}

} // namespace ac
