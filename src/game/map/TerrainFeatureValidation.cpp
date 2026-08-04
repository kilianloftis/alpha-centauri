#include "game/map/TerrainFeatureValidation.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/Tile.h"

#include <magic_enum.hpp>
#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

// Every enumerator of TEnum names an improvement id Tile looks up by magic_enum name.
template <typename TEnum>
void RequireImprovementEntries_(const ImprovementRegistry& rImprovements, const char* what)
{
    for (const TEnum value : magic_enum::enum_values<TEnum>())
    {
        const std::string id(magic_enum::enum_name(value));
        if (!rImprovements.Find(id))
        {
            throw std::runtime_error("improvements.json is missing the " + std::string(what)
                                     + " entry '" + id + "' required by Tile");
        }
    }
}

} // namespace

void ValidateTerrainFeatures(const ImprovementRegistry& rImprovements)
{
    RequireImprovementEntries_<Rockiness_t>(rImprovements, "rockiness");
    RequireImprovementEntries_<Moisture_t>(rImprovements, "moisture");
    RequireImprovementEntries_<TerrainFeature_t>(rImprovements, "terrain feature");
}

} // namespace ac
