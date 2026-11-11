#include "game/supply/SupplyPodIndex.h"

#include "game/map/Tile.h"

#include <cstdint>

namespace ac
{

std::uint64_t SupplyPodIndex::Key_(const Tile& rTile)
{
    const auto x = static_cast<std::uint64_t>(static_cast<std::uint32_t>(rTile.GetX()));
    const auto y = static_cast<std::uint64_t>(static_cast<std::uint32_t>(rTile.GetY()));
    return (x << 32) | y;
}

void SupplyPodIndex::Stamp(const Tile& rTile, std::string eventGroupId)
{
    m_eventGroupByTile.insert_or_assign(Key_(rTile), std::move(eventGroupId));
}

const std::string* SupplyPodIndex::Find(const Tile& rTile) const
{
    const auto it = m_eventGroupByTile.find(Key_(rTile));
    if (it == m_eventGroupByTile.end())
    {
        return nullptr;
    }
    return &it->second;
}

void SupplyPodIndex::Clear(const Tile& rTile)
{
    m_eventGroupByTile.erase(Key_(rTile));
}

} // namespace ac
