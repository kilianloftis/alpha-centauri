#pragma once

#include "game/units/UnitDomain.h"

#include <array>
#include <cstddef>

namespace ac
{

// Named interaction matrices loaded from config/interaction_grids.json.
//
// Every grid has the same shape: the acting unit's domain (the row) against one other
// thing (the column). "Actor" is always the unit whose own InteractionOverrides are
// consulted first — the mover, the attacker, the ZOC projector. Only the column vocabulary
// differs between grids, so each grid names its column for what that column actually is.
enum class InteractionGridId_t
{
    Enter,      // actor_domain x surface
    AttackTile, // actor_domain x footing
    AttackUnit, // actor_domain x target_domain
    Zoc,        // actor_domain x target_domain
};

enum class InteractionCell_t
{
    Allow,
    Deny,
};

// Enter-grid column: the target tile's surface (not a UnitDomain).
enum class InteractionSurface_t
{
    Land,
    Water,
};

// AttackTile-grid column: what the attacker is standing on. Like every other axis this is read
// straight off the entity rather than derived from a rule — Embarked is simply "aboard a
// carrier", which is distinct from Water because a land unit garrisoning a sea base is on
// water without being aboard anything.
enum class InteractionFooting_t
{
    Land,
    Water,
    Embarked,
};

inline constexpr std::size_t k_UnitDomainCount = 4;
inline constexpr std::size_t k_SurfaceCount = 2;
inline constexpr std::size_t k_FootingCount = 3;

inline std::size_t UnitDomainIndex(UnitDomain_t domain)
{
    return static_cast<std::size_t>(domain);
}

inline std::size_t SurfaceIndex(InteractionSurface_t surface)
{
    return static_cast<std::size_t>(surface);
}

inline std::size_t FootingIndex(InteractionFooting_t footing)
{
    return static_cast<std::size_t>(footing);
}

// Bitmask over InteractionGridId_t. Cached on effect sources (designs, faction pools) so a
// resolve can skip effect collection entirely when the source carries no InteractionOverride
// for the grid being queried — which is the common case.
using InteractionGridMask_t = unsigned char;

inline constexpr InteractionGridMask_t GridBit(InteractionGridId_t grid)
{
    return static_cast<InteractionGridMask_t>(1u << static_cast<unsigned>(grid));
}

inline constexpr bool MaskCovers(InteractionGridMask_t mask, InteractionGridId_t grid)
{
    return (mask & GridBit(grid)) != 0;
}

struct InteractionGridsConfig_t
{
    // [actor_domain][surface]
    std::array<std::array<InteractionCell_t, k_SurfaceCount>, k_UnitDomainCount> enter{};
    // [actor_domain][footing]
    std::array<std::array<InteractionCell_t, k_FootingCount>, k_UnitDomainCount> attackTile{};
    // [actor_domain][target_domain]
    std::array<std::array<InteractionCell_t, k_UnitDomainCount>, k_UnitDomainCount> attackUnit{};
    // [actor_domain][target_domain]
    std::array<std::array<InteractionCell_t, k_UnitDomainCount>, k_UnitDomainCount> zoc{};
};

} // namespace ac
