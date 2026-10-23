#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace ac
{

// An empty requirement is always met; any other needs the tech discovered.
inline bool MeetsTechRequirement(const std::string& rRequiredTech,
                                 const std::vector<std::string>& rDiscovered)
{
    return rRequiredTech.empty() || std::ranges::contains(rDiscovered, rRequiredTech);
}

} // namespace ac
