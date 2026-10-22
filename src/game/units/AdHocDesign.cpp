#include "game/units/AdHocDesign.h"

#include "game/faction/Faction.h"
#include "game/units/Military.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitSlotConfig.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace ac
{

const UnitDesign* EnsureAdHocDesign(Faction& rFaction, const UnitComponentRegistry& rComponents,
                                    std::span<const std::string> componentIds,
                                    std::string_view slotPrefix)
{
    if (componentIds.empty())
    {
        return nullptr;
    }

    std::vector<UnitSlotConfig_t> slots;
    std::unordered_map<std::string, const UnitComponentConfig_t*> assigned;
    int slotIndex = 0;
    for (const std::string& rId : componentIds)
    {
        const UnitComponentConfig_t& rComponent = rComponents.Get(rId);
        UnitSlotConfig_t slot;
        slot.id = std::string(slotPrefix) + "_slot_" + std::to_string(slotIndex++);
        slot.displayName = slot.id;
        slot.componentType = rComponent.type;
        slot.required = true;
        assigned[slot.id] = &rComponent;
        slots.push_back(slot);
    }

    auto pDesign = std::make_unique<UnitDesign>(slots, assigned);
    Military& rMilitary = rFaction.GetMilitary();
    if (const IDesign* pExisting = rMilitary.GetDesign(pDesign->GetId()))
    {
        return dynamic_cast<const UnitDesign*>(pExisting);
    }
    const UnitDesign* pAdded = pDesign.get();
    rMilitary.AddDesign(std::move(pDesign));
    return pAdded;
}

} // namespace ac
