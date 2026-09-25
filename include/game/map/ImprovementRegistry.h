#pragma once

#include "game/map/ImprovementConfigParser.h"
#include "lib/Registry.h"

namespace ac
{

// Every tile occupant: buildable improvements and terrain features alike, told apart by
// ImprovementConfig_t::placement. One registry because both answer the same questions —
// what effects do they grant, and what do they exclude — and because a single id space
// makes "this id is both" impossible rather than a runtime check.
class ImprovementRegistry : public Registry<ImprovementConfig_t, ImprovementConfigParser>
{
public:
    // Occupants only, for callers with no use for former projects. The two files are parsed
    // and tag-expanded together before either is installed, so an improvement may name a @tag
    // that only terrain entries carry. An id in both files is a duplicate, and Validate_
    // rejects it. Anything that also needs projects uses LoadMapOccupants.
    void LoadOccupants(const std::string& rImprovementsPath, const std::string& rTerrainPath)
    {
        Assign(LoadTileOccupants(rImprovementsPath, rTerrainPath));
    }

private:
    // Hidden: loading improvements.json on its own leaves every terrain id unresolvable and
    // expands @tags over half the occupant list. LoadOccupants and LoadMapOccupants are the
    // only ways in, so a half-loaded occupant space cannot be reached by accident.
    using Registry<ImprovementConfig_t, ImprovementConfigParser>::Load;
};

} // namespace ac
