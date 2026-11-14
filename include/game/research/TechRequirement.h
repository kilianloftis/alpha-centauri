#pragma once

#include "game/RequirementLists.h"

#include <algorithm>
#include <string>
#include <vector>

namespace ac
{

struct TechRequirement_t
{
    std::string tech;
};

using RequiredTechs_t = RequirementLists_t<TechRequirement_t>;

inline bool HasTechRequirements(const RequiredTechs_t& rRequired)
{
    return !rRequired.any.empty() || !rRequired.all.empty();
}

// Empty any/all is always met; otherwise every all and at least one any must be discovered.
inline bool MeetsTechRequirement(const RequiredTechs_t& rRequired,
                                 const std::vector<std::string>& rDiscovered)
{
    return MeetsRequirementLists(rRequired, [&](const TechRequirement_t& rEntry)
    {
        return std::ranges::contains(rDiscovered, rEntry.tech);
    });
}

// Test / fixture helper: one-tech any list (empty string → no requirement).
inline RequiredTechs_t RequireAnyTech(std::string tech)
{
    RequiredTechs_t required;
    if (!tech.empty())
    {
        required.any.push_back(TechRequirement_t{std::move(tech)});
    }
    return required;
}

} // namespace ac
