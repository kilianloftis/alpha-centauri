#pragma once

namespace ac
{

// Which facilities a random destruction may not pick. Facilities whose loss would raze the
// base (WouldDestroyingBuildingRazeBase) are always omitted by CollectDestroyableFacilities.
struct FacilityExclusions_t
{
    bool excludeHq = false;
    bool excludeSecretProjects = false;
};

} // namespace ac
