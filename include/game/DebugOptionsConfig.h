#pragma once

namespace ac
{

// Session debug knobs (not part of save-game serialization).
struct DebugOptionsConfig_t
{
    // When set, the player faction sees through fog of war (full current vision).
    bool removeFog = false;
};

} // namespace ac
