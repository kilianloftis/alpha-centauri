#pragma once

namespace ac
{

// Session visibility knobs (not part of save-game serialization).
struct VisibilityConfig_t
{
    // When set, every faction treats the whole map as explored (no shroud).
    bool removeShroud = false;
    // When set, the player faction sees through fog of war (full current vision).
    bool removeFog = false;

    bool operator==(const VisibilityConfig_t&) const = default;
};

} // namespace ac
