#pragma once

#include "game/map/ImprovementConfigParser.h"
#include "lib/Registry.h"

namespace ac
{

// Every tile occupant, improvement or terrain, in one id space so no id can be both.
class ImprovementRegistry : public Registry<ImprovementConfig_t, ImprovementConfigParser>
{
public:
    // Occupants only, parsed and tag-expanded together. LoadMapOccupants also loads projects.
    void LoadOccupants(const std::string& rImprovementsPath, const std::string& rTerrainPath)
    {
        Assign(LoadTileOccupants(rImprovementsPath, rTerrainPath));
    }

private:
    // Hidden, so improvements.json cannot be loaded without terrain.
    using Registry<ImprovementConfig_t, ImprovementConfigParser>::Load;
};

} // namespace ac
