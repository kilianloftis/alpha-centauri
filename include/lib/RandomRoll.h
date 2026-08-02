#pragma once

#include <random>

namespace ac
{

// True with a chancePercent-in-100 probability (50 = 50%).
// Certainties are resolved without touching rRng: <= 0 never fires, >= 100 always fires.
inline bool RollPercent(int chancePercent, std::mt19937& rRng)
{
    if (chancePercent <= 0)
    {
        return false;
    }
    if (chancePercent >= 100)
    {
        return true;
    }
    std::uniform_int_distribution<int> dist(1, 100);
    return dist(rRng) <= chancePercent;
}

} // namespace ac
