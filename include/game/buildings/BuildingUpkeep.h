#pragma once

#include "game/buildings/BuildingConfig.h"
#include "game/effects/ActiveEffect.h"

#include <span>
#include <vector>

namespace ac
{

// One building type's upkeep line for the UI. Only constructed copies count, never grants.
struct BuildingUpkeepLine_t
{
    const BuildingConfig_t* pConfig = nullptr;
    int count = 0;
    // Resolved FacilityEnergyUpkeep (RawScaled from config base + matching mods). Set by tally.
    int upkeepPerCopy = 0;

    int UpkeepPerCopy() const { return upkeepPerCopy; }
    int TotalUpkeep() const { return count * upkeepPerCopy; }
};

// One copy's energy upkeep, context-free. Base-tagged scopes need a matching pOriginBase.
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
