#pragma once

#include "game/units/UnitDomain.h"

#include <array>
#include <cstddef>
#include <string_view>
#include <utility>

namespace ac
{

// Interaction matrices from config/interaction_grids.json: the acting unit's domain (row)
// against one other thing (column). The actor's own overrides are consulted first.
enum class InteractionGridId_t
{
    Enter,      // actor_domain x surface
    AttackTile, // actor_domain x footing
    AttackUnit, // actor_domain x target_domain
    Zoc,        // actor_domain x target_domain
};

// Wire names of the grids, which are not their enumerator names.
inline constexpr std::array<std::pair<std::string_view, InteractionGridId_t>, 4>
    k_InteractionGridNames{{{"enter", InteractionGridId_t::Enter},
                            {"attack_tile", InteractionGridId_t::AttackTile},
                            {"attack_unit", InteractionGridId_t::AttackUnit},
                            {"zoc", InteractionGridId_t::Zoc}}};

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

// AttackTile column: what the attacker stands on. Embarked is "aboard a carrier", not Water.
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

// Grids a source carries overrides for, so the common case skips effect collection.
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
