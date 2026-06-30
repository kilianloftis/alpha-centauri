#pragma once

#include "game/units/UnitSlotConfig.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace ac
{

struct UnitComponentConfig_t;

struct UnitDesignerState_t
{
    std::unordered_map<std::string, const UnitComponentConfig_t*> components; // slotId → component

    bool HasAllMandatory(const std::vector<UnitSlotConfig_t>& rSlots) const
    {
        for (const auto& rSlot : rSlots)
        {
            if (rSlot.required)
            {
                auto it = components.find(rSlot.id);
                if (it == components.end() || !it->second)
                {
                    return false;
                }
            }
        }
        return true;
    }
};

} // namespace ac
