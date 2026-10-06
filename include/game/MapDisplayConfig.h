#pragma once

namespace ac
{

// How the world map shows elevation: not at all, following elevation continuously, or in SMAC's
// whole altitude levels.
enum class ReliefMode_t
{
    Flat,
    Smooth,
    Stepped,
};

// Player preferences for how the world map is drawn.
struct MapDisplayConfig_t
{
    ReliefMode_t relief = ReliefMode_t::Smooth;
    // Grid lines along edges that touch water.
    bool bOceanGrid = false;

    bool operator==(const MapDisplayConfig_t&) const = default;
};

} // namespace ac
