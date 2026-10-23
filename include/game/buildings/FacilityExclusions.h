#pragma once

namespace ac
{

// Which facilities a random destruction may not pick.
struct FacilityExclusions_t
{
    bool excludeHq = false;
    bool excludeSecretProjects = false;
};

} // namespace ac
