#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace ac
{

class Tile;

// Per-tile event-group id stamped when a supply pod is placed. Cleared when the pod opens.
class SupplyPodIndex
{
public:
    void Stamp(const Tile& rTile, std::string eventGroupId);
    // Null when the tile has no stamp.
    const std::string* Find(const Tile& rTile) const;
    void Clear(const Tile& rTile);

private:
    static std::uint64_t Key_(const Tile& rTile);

    std::unordered_map<std::uint64_t, std::string> m_eventGroupByTile;
};

} // namespace ac
