#pragma once

#include "game/effects/EffectConfig.h"
#include "game/effects/TriggeredEffect.h"
#include "game/faction/base/production/ScrapConfig.h"
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
    // Absent (empty) means any chassis. A present list is the only chassis ids this component
    // may be designed with.
    std::vector<std::string> requiresChassis;
    std::vector<EffectConfig_t> effects;
    // One-shot effects fired once, when a unit carrying this component finishes production
    // (the colony pod's population cost). Free spawns never pay these — see BaseManager.
    std::vector<TriggeredEffectConfig_t> onCompleteEffects;
    // Considered when a unit carrying this component is ordered to Hold, and again when a
    // building is completed on its tile. UnitDesign gathers these in slot order.
    std::vector<TriggeredEffectConfig_t> onHoldEffects;
    // Fired when a unit carrying this component is ordered to detonate in place — the
    // delivery path for a warhead that cannot declare an attack. A DestroyUnit entry spends
    // the carrier. UnitDesign gathers these in slot order.
    std::vector<TriggeredEffectConfig_t> onDetonateEffects;
    // Optional partial override of kinds.unit.default_scrap, folded per design by
    // MergeScrapOverride: later occupied slots win on a given key. `"formula": null` denies
    // scrap.
    std::optional<ScrapOverride_t> scrap;
    // Display-only annotations for FormatCombatRating (not gameplay effects).
    std::vector<CombatRatingModifier_t> combatRatingModifiers;
    std::vector<std::string> combatRatingLabels;

    // Empty requiredTech = always available (matches BuildingConfig_t::IsAvailable).
    bool IsAvailable(const std::vector<std::string>& rDiscoveredTechs) const
    {
        if (requiredTech.empty())
        {
            return true;
        }
        return std::find(rDiscoveredTechs.begin(), rDiscoveredTechs.end(), requiredTech)
               != rDiscoveredTechs.end();
    }
};

// Empty requiresChassis allows every chassis. Otherwise rChassisId must be in the list.
inline bool ChassisRequirementMet(const UnitComponentConfig_t& rComponent,
                                  const std::string& rChassisId)
{
    if (rComponent.requiresChassis.empty())
    {
        return true;
    }
    return std::find(rComponent.requiresChassis.begin(), rComponent.requiresChassis.end(),
                     rChassisId) != rComponent.requiresChassis.end();
}

} // namespace ac
