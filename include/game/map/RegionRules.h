#pragma once

#include <vector>

namespace ac
{

class WorldMap;

// SMAC's land and water bodies (docs/thinker/terranx-map.md "Regions", without its cap), one id
// per tile in GetTiles() order. Off the first and last rows, same-surface tiles that touch share
// an id; each of those two rows takes one id per surface. Land and water never share.
std::vector<int> AssignRegions(const WorldMap& rMap);

} // namespace ac
