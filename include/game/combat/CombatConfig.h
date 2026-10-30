#pragma once

namespace ac
{

// Combat tunables that no effect modifies. Loaded from config/combat.json.
struct CombatConfig_t
{
    // Smallest population the nearest base needs to become a boarded unit's home base.
    int boardingHomeBaseMinPop = 0;
};

} // namespace ac
