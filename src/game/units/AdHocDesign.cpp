#include "game/units/AdHocDesign.h"

#include "game/Faction.h"
#include "game/GameDataContext.h"
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

const UnitDesign* EnsureAdHocDesign(Faction& rFaction, const GameDataContext& rDataContext,
                                    std::span<const std::string> componentIds,
                                    std::string_view slotPrefix)
{
    if (componentIds.empty())
    {
        return nullptr;
    }
    // These throws should be unreachable: every caller's ids are validated against the registry
    // at load (GameDataContext), so a typo fails at startup naming the file rather than here,
    // mid-rule, after other effects have already applied. Kept as assertions for a context
    // assembled by hand.
    if (!rDataContext.unitComponentRegistry)
    {
        throw std::runtime_error(
            "EnsureAdHocDesign: no unit component registry is available to assemble '"
            + std::string(slotPrefix) + "'");
    }

    std::vector<UnitSlotConfig_t> slots;
    std::unordered_map<std::string, const UnitComponentConfig_t*> assigned;
    int slotIndex = 0;
    for (const std::string& rId : componentIds)
    {
        const UnitComponentConfig_t* pComponent = rDataContext.unitComponentRegistry->Find(rId);
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
