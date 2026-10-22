#pragma once

namespace ac
{

// Movement and ZOC domain, set by a unit's chassis.
enum class UnitDomain_t
{
    Land,
    Sea,
    Air,
    // Missiles / orbital strike chassis. Tile entry like Air; not a building orbital flag.
    Orbital,
};

} // namespace ac
