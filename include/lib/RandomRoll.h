#pragma once

#include "lib/Rational.h"

#include <random>
#include <stdexcept>

namespace ac
{

// SMAC game_randv(n): a draw in [0, exclusiveMax) when exclusiveMax > 1, else 0.
inline int RollExclusiveMax(int exclusiveMax, std::mt19937& rRng)
{
    if (exclusiveMax <= 1)
    {
        return 0;
    }
    std::uniform_int_distribution<int> dist(0, exclusiveMax - 1);
    return dist(rRng);
}

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

// True with probability numerator/denominator. Certainties skip the RNG: numerator <= 0 never
// fires; numerator >= denominator always fires. Denominator must be positive (Rational_t rule).
inline bool RollRational(const Rational_t& rChance, std::mt19937& rRng)
{
    if (rChance.denominator <= 0)
    {
        throw std::runtime_error("RollRational: denominator must be positive");
    }
    if (rChance.numerator <= 0)
    {
        return false;
    }
    if (rChance.numerator >= rChance.denominator)
    {
        return true;
    }
    std::uniform_int_distribution<int> dist(1, rChance.denominator);
    return dist(rRng) <= rChance.numerator;
}

} // namespace ac
