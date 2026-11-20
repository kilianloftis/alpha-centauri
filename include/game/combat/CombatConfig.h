#pragma once

#include "game/effects/EffectConfig.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace ac
{

// The kind of fight a combat rating is resolved for.
enum class CombatKindId_t
{
    Conventional,
    Psi,
};

// Combat tunables and standing combat rules. Loaded from config/combat.json.
struct CombatConfig_t
{
    // Smallest population the nearest base needs to become a boarded unit's home base.
    int boardingHomeBaseMinPop = 0;
    // Each kind of fight's defense slots (combat.json defense_slots), in file order. A `defense`
    // modifier naming a slot resolves in that slot's own stack, read only by these kinds.
    std::map<CombatKindId_t, std::vector<StatSlot_t>> defenseSlotsByKind;
    // Standing WorldGlobal combat rules, such as the psi values.
    std::vector<EffectConfig_t> effects;
};

inline bool DeclaresDefenseSlot(const CombatConfig_t& rConfig, const std::string& rSlotId)
{
    return std::ranges::any_of(rConfig.defenseSlotsByKind, [&](const auto& rEntry) {
        return std::ranges::find(rEntry.second, rSlotId, &StatSlot_t::id) != rEntry.second.end();
    });
}

} // namespace ac
