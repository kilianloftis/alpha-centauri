#pragma once

namespace ac
{

// Which facilities a random destruction may not pick.
struct FacilityExclusions_t
{
    bool excludeHq = false;
    bool excludeSecretProjects = false;
    // SMAC capture_base skips Pressure Dome (facility 0x1A) in the 1/3 loop.
    bool excludePressureDome = false;
};

} // namespace ac
