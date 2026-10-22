#pragma once

#include "game/effects/EffectConfig.h"
#include "game/effects/TriggeredEffect.h"
#include "game/production/ScrapConfig.h"
#include "game/units/UnitDomain.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace ac
{

// Where a combat-rating annotation attaches on the SMAC-style A-D-M string.
enum class CombatRatingTarget_t
{
    Attack,
    Defense,
    Movement,
    Rating, // the completed A-D-M string (e.g. reactor "*2")
};

struct CombatRatingModifier_t
{
    CombatRatingTarget_t target = CombatRatingTarget_t::Attack;
    std::string prefix;
    std::string suffix;
};

struct UnitComponentConfig_t
{
    std::string id;
    std::string name;
    // Fragment used in UnitDesign display names (Weapon Armour Chassis). May differ from
    // name (e.g. Hand Weapons → "Scout"); empty omits this component from the design name.
    std::string unitName;
    std::string type; // e.g. "chassis", "weapon" — matches component_type in unit_slot_config.json
    std::string requiredTech;
    int mineralCost = 0;
    // Required when type == "chassis"; must be unset for other component types.
    std::optional<UnitDomain_t> domain;
    // The only chassis this component may be designed on; empty means any.
    std::vector<std::string> requiresChassis;
    std::vector<EffectConfig_t> effects;
    // Fired once when a unit with this component finishes production (a colony pod's pop cost).
    std::vector<TriggeredEffectConfig_t> onCompleteEffects;
    // Considered when a unit carrying this component is ordered to Hold, and again when a
    // building is completed on its tile. UnitDesign gathers these in slot order.
    std::vector<TriggeredEffectConfig_t> onHoldEffects;
    // Fired when the unit is ordered to detonate in place, for a warhead that cannot attack.
    std::vector<TriggeredEffectConfig_t> onDetonateEffects;
    // Overrides kinds.unit.default_scrap per key, later slots winning. A null formula denies
    // scrap.
    std::optional<ScrapOverride_t> scrap;
    // Display-only annotations for FormatCombatRating (not gameplay effects).
    std::vector<CombatRatingModifier_t> combatRatingModifiers;
    std::vector<std::string> combatRatingLabels;

    bool IsAvailable(const std::vector<std::string>& rDiscoveredTechs) const
    {
        return requiredTech.empty() || std::ranges::contains(rDiscoveredTechs, requiredTech);
    }
};

inline bool ChassisRequirementMet(const UnitComponentConfig_t& rComponent,
                                  const std::string& rChassisId)
{
    return rComponent.requiresChassis.empty()
           || std::ranges::contains(rComponent.requiresChassis, rChassisId);
}

} // namespace ac
