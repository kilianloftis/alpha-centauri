#include "game/faction/base/production/ProductionPauseGates.h"

#include "game/IConstructable.h"
#include "game/units/UnitDesign.h"

#include <stdexcept>

namespace ac
{

std::vector<PauseOnEventId_t> CollectProductionPauseGates(const IConstructable& rItem,
                                                          bool bPrototype)
{
    std::vector<PauseOnEventId_t> gates;

    switch (rItem.GetConstructableKind())
    {
    case ConstructableKind_t::Building:
    case ConstructableKind_t::SecretProject:
        gates.push_back(PauseOnEventId_t::NewFacilityBuilt);
        break;
    case ConstructableKind_t::Unit:
    {
        const auto* pDesign = dynamic_cast<const UnitDesign*>(&rItem);
        if (pDesign && pDesign->IsCombatUnit())
        {
            gates.push_back(PauseOnEventId_t::CombatUnitBuilt);
        }
        else
        {
            gates.push_back(PauseOnEventId_t::NonCombatUnitBuilt);
        }
        if (bPrototype)
        {
            gates.push_back(PauseOnEventId_t::PrototypeBuilt);
        }
        break;
    }
    case ConstructableKind_t::Stockpile:
        throw std::logic_error("CollectProductionPauseGates: a stockpile cannot complete");
    }

    return gates;
}

} // namespace ac
