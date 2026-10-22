#pragma once

#include "lib/Rational.h"

namespace ac
{

// Movement-point subdivision: feature costs are stored in fragments so roads at "1/3" land on
// whole ones.
struct MovementConstants_t
{
    // Divisible by 2,3,4,5,6,8,9,10 — road "1/3" → 120 fragments.
    static constexpr int k_moveFragmentsPerPoint = 360;

    // Default move_cost when an improvement/terrain entry omits the field (1 move point).
    Rational_t defaultMoveCost{1, 1};

    int DefaultMoveCostFragments() const
    {
        return defaultMoveCost.ScaledInt(k_moveFragmentsPerPoint);
    }
};

} // namespace ac
