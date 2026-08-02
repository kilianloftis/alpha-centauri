#pragma once

namespace ac
{

// Movement / ZOC domain. Required on chassis components; land = neither sea nor air.
enum class UnitDomain_t
{
    Land,
    Sea,
    Air,
    // Missiles / orbital strike chassis. Tile entry like Air; not a building orbital flag.
    Orbital,
};

} // namespace ac
