#pragma once

#include "game/combat/CombatRules.h"
#include "game/diplomacy/DiplomaticStatus.h"
#include "game/faction/Faction.h"
#include "game/units/Unit.h"

#include <string>

namespace ac
{

// The odds rAttacker would see before attacking rDefender.
inline std::string FormatAttackConfirm(const Unit& rAttacker, const Unit& rDefender)
{
    const CombatOdds_t odds = ComputeCombatOdds(rAttacker, rDefender, CombatEngagement_t::Standard);
    return "Their odds are " + std::to_string(odds.attackerOdds) + " to "
           + std::to_string(odds.defenderOdds) + ". Proceed?";
}

// Asked before an attack on rVictim that breaks `broken` (StatusBrokenByAttack). Breaking
// Neutral pronounces Vendetta.
inline std::string FormatStatusBreakConfirm(DiplomaticStatus_t broken, const Faction& rVictim)
{
    const std::string& rName = rVictim.GetDefinition().identity.name;
    if (broken == DiplomaticStatus_t::Neutral)
    {
        return "Do you wish to pronounce Vendetta upon the " + rName + "?";
    }
    return "We hold a " + ToString(broken) + " with the " + rName + ". Break the "
           + ToString(broken) + "?";
}

// Why an attack on rVictim is refused when it would break `broken` (MayBreakByAttack).
inline std::string FormatStatusBreakRefusal(DiplomaticStatus_t broken, const Faction& rVictim)
{
    return "We hold a " + ToString(broken) + " with the "
           + rVictim.GetDefinition().identity.name + ". It can only be ended in talks with them;"
           + " until then we can take no hostile action.";
}

} // namespace ac
