#pragma once

#include "game/buildings/BuildingConfig.h"
#include "game/effects/ActiveEffect.h"

#include <span>
#include <vector>

namespace ac
{

// One building type's contribution to energy-credit upkeep (owned copy count × effective
// per-copy upkeep). Built for UI: name/id via pConfig, resolved per-copy cost, and line total.
// Only constructed buildings count — continuous GrantBuilding expansions (e.g. Command Nexus)
// never enter BuildingManager and therefore never appear here.
struct BuildingUpkeepLine_t
{
    const BuildingConfig_t* pConfig = nullptr;
    int count = 0;
    // Resolved FacilityEnergyUpkeep (RawScaled from config base + matching mods). Set by tally.
    int upkeepPerCopy = 0;

    int UpkeepPerCopy() const { return upkeepPerCopy; }
    int TotalUpkeep() const { return count * upkeepPerCopy; }
};

// Effective energy upkeep for one copy of rBuilding given the faction's active effects.
// Context-free (skips conditioned effects). TagsOriginBase scopes apply only when
// pOriginBase is non-null and matches ActiveEffect_t::originBase.
int ResolveFacilityEnergyUpkeepPerCopy(const BuildingConfig_t& rBuilding,
                                       std::span<const ActiveEffect_t> rEffects,
                                       const BaseManager* pOriginBase = nullptr);

// Group building copies by id; resolve upkeepPerCopy via FacilityEnergyUpkeep modifiers.
// Lines are sorted ascending by id for stable UI order.
std::vector<BuildingUpkeepLine_t> TallyBuildingUpkeepByType(
    const std::vector<const BuildingConfig_t*>& rBuildings,
    std::span<const ActiveEffect_t> rEffects,
    const BaseManager* pOriginBase = nullptr);

// Sum of TotalUpkeep() across lines.
int SumBuildingUpkeep(const std::vector<BuildingUpkeepLine_t>& rLines);

} // namespace ac
