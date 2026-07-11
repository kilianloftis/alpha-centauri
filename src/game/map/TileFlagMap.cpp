#include "game/map/TileFlagMap.h"

#include "game/map/Tile.h"
#include <algorithm>

namespace ac
{

void TileFlagMap::Reset(int width, int height)
{
    m_width = width;
    m_height = height;
    const size_t count = (width > 0 && height > 0)
        ? static_cast<size_t>(width) * static_cast<size_t>(height)
        : 0;
    m_flags.assign(count, 0);
    m_revision.Bump();
}

void TileFlagMap::ClearAll()
{
    if (!IsSized())
    {
        return;
    }
    std::fill(m_flags.begin(), m_flags.end(), 0);
    m_revision.Bump();
}

bool TileFlagMap::InBounds_(int x, int y) const
{
    return x >= 0 && y >= 0 && x < m_width && y < m_height;
}

size_t TileFlagMap::Index_(int x, int y) const
{
    return static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x);
}

bool TileFlagMap::Test(int x, int y) const
{
    return IsSized() && InBounds_(x, y) && m_flags[Index_(x, y)] != 0;
}

bool TileFlagMap::Test(const Tile& rTile) const
{
    return Test(rTile.GetX(), rTile.GetY());
}

void TileFlagMap::Set(int x, int y)
{
    if (!IsSized() || !InBounds_(x, y))
    {
        return;
    }
    
    if (!m_flags[Index_(x, y)])
    {
        m_flags[Index_(x, y)] = 1;
        m_revision.Bump();
    }
}

void TileFlagMap::Set(const Tile& rTile)
{
    Set(rTile.GetX(), rTile.GetY());
}

} // namespace ac
