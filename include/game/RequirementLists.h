#pragma once

#include <vector>

namespace ac
{

// Shared any / all / none lists. Domain entries differ (tech id vs building+scope+count).
// Empty lists = no clause. Omit empty lists in JSON.
template <typename Entry_t>
struct RequirementLists_t
{
    std::vector<Entry_t> any;
    std::vector<Entry_t> all;
    std::vector<Entry_t> none;
};

// all must hold; if any is non-empty at least one must; none must not hold.
template <typename Entry_t, typename MetFn>
bool MeetsRequirementLists(const RequirementLists_t<Entry_t>& rLists, MetFn&& met)
{
    for (const Entry_t& rEntry : rLists.all)
    {
        if (!met(rEntry))
        {
            return false;
        }
    }
    if (!rLists.any.empty())
    {
        bool bAnyMet = false;
        for (const Entry_t& rEntry : rLists.any)
        {
            if (met(rEntry))
            {
                bAnyMet = true;
                break;
            }
        }
        if (!bAnyMet)
        {
            return false;
        }
    }
    for (const Entry_t& rEntry : rLists.none)
    {
        if (met(rEntry))
        {
            return false;
        }
    }
    return true;
}

} // namespace ac
