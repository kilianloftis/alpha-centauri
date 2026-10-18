#pragma once

#include <random>

namespace ac
{

class MoraleCalculator;
class Unit;

// Promote the survivor one intrinsic XP on success. Caps at the top level.
bool TryPromote(Unit& rSurvivor, int attackStrength, int defenseStrength,
                const MoraleCalculator& rMorale, std::mt19937& rRng);

} // namespace ac
