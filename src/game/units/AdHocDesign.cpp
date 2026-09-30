#include "game/units/AdHocDesign.h"

#include "game/Faction.h"
#include "game/faction/Military.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitSlotConfig.h"

#include <memory>
#include <stdexcept>
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
        // Unreachable for validated ids: every caller's ids are checked against the registry at
        // load, so a typo fails at startup naming the file rather than here, mid-rule.
        const UnitComponentConfig_t* pComponent = rComponents.Find(rId);
        if (!pComponent)
        {
            throw std::runtime_error("EnsureAdHocDesign: component '" + rId
                                     + "' is not in the unit component registry");
        }
        UnitSlotConfig_t slot;
        slot.id = std::string(slotPrefix) + "_slot_" + std::to_string(slotIndex++);
        slot.displayName = slot.id;
        slot.componentType = pComponent->type;
        slot.required = true;
        assigned[slot.id] = pComponent;
        slots.push_back(slot);
    }

    auto pDesign = std::make_unique<UnitDesign>(slots, assigned);
    const std::string designId = pDesign->GetId();
    if (const IDesign* pExisting = rFaction.GetMilitary().GetDesign(designId))
    {
        return dynamic_cast<const UnitDesign*>(pExisting);
    }
    rFaction.GetMilitary().AddDesign(std::move(pDesign));
    return dynamic_cast<const UnitDesign*>(rFaction.GetMilitary().GetDesign(designId));
}

} // namespace ac
