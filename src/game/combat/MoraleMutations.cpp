#include "game/combat/MoraleMutations.h"

#include "game/combat/MoraleCalculator.h"
#include "game/units/Unit.h"

namespace ac
{

bool TryPromote(Unit& rSurvivor, int attackStrength, int defenseStrength,
                const MoraleCalculator& rMorale, std::mt19937& rRng)
{
    const int xp = rSurvivor.GetXp();
    // No draw for a promotion that cannot happen, so the session RNG stream is unaffected.
    if (xp >= rMorale.GetConfig().MaxLevel())
    {
        return false;
    }
    const double chance = rMorale.PromotionChance(xp, attackStrength, defenseStrength);

    std::uniform_real_distribution<double> dist(0.0, 1.0);
    if (dist(rRng) >= chance)
    {
        return false;
    }
    rSurvivor.SetXp(xp + 1);
    return true;
}

} // namespace ac
