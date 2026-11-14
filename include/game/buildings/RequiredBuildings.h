#pragma once

#include "game/RequirementLists.h"

#include <string>

namespace ac
{

// Where a required-buildings entry is counted. Required in config (no inference).
enum class BuildingRequirementScope_t
{
    ThisBase,
    ThisFaction,
    World,
};

// One building id counted at a scope. count is "at least N" (default 1).
struct BuildingRequirement_t
{
    std::string building;
    BuildingRequirementScope_t scope = BuildingRequirementScope_t::ThisBase;
    int count = 1;
};

using RequiredBuildings_t = RequirementLists_t<BuildingRequirement_t>;

} // namespace ac
