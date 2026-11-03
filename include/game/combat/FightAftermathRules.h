#pragma once

#include "game/base/BaseTypes.h"

#include <optional>

namespace ac
{

class Tile;

// The faction a fight on rBattleTile (the defender's tile) earns the attacker goodwill with: the
// tile's territory owner, land or water, when it is neither side, holds Vendetta with defender,
// and holds an agreement with attacker. nullopt otherwise, including an unowned tile.
std::optional<FactionId_t> AlliedTerritoryOwner(FactionId_t attacker, FactionId_t defender,
                                                const Tile& rBattleTile);

} // namespace ac
