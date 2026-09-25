#include "game/map/TerrainFeatureValidation.h"

#include "game/map/ImprovementIds.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/Tile.h"

#include <magic_enum.hpp>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ac
{

namespace
{

const char* FileFor_(OccupantPlacement_t placement)
{
    return placement == OccupantPlacement_t::Terrain ? "config/terrain.json"
                                                     : "config/improvements.json";
}

void RequireOccupant_(const ImprovementRegistry& rOccupants, std::string_view id,
                      OccupantPlacement_t placement, const std::string& rWhy)
{
    const ImprovementConfig_t* pConfig = rOccupants.Find(std::string(id));
    if (!pConfig)
    {
        throw std::runtime_error(std::string(FileFor_(placement)) + " is missing the entry '"
                                 + std::string(id) + "' required by " + rWhy);
    }
    if (pConfig->placement != placement)
    {
        throw std::runtime_error("'" + std::string(id) + "' must be declared in "
                                 + FileFor_(placement) + ", because it is required by " + rWhy);
    }
}

// Every enumerator of TEnum names a terrain occupant Tile looks up by magic_enum name.
template <typename TEnum>
void RequireTerrainEnum_(const ImprovementRegistry& rOccupants, const char* pWhat)
{
    for (const TEnum value : magic_enum::enum_values<TEnum>())
    {
        RequireOccupant_(rOccupants, magic_enum::enum_name(value), OccupantPlacement_t::Terrain,
                         std::string(pWhat) + " mirroring on Tile");
    }
}

template <typename TEnum>
bool NamesEnumerator_(std::string_view id)
{
    return magic_enum::enum_cast<TEnum>(id).has_value();
}

} // namespace

bool IsDerivedTerrainId(std::string_view id)
{
    if (NamesEnumerator_<Rockiness_t>(id) || NamesEnumerator_<Moisture_t>(id))
    {
        return true;
    }
    const auto feature = magic_enum::enum_cast<TerrainFeature_t>(id);
    return feature
        && (*feature == TerrainFeature_t::Water || *feature == TerrainFeature_t::Ocean
            || *feature == TerrainFeature_t::OceanShelf);
}

void ValidateTerrainFeatures(const ImprovementRegistry& rOccupants)
{
    RequireTerrainEnum_<Rockiness_t>(rOccupants, "rockiness");
    RequireTerrainEnum_<Moisture_t>(rOccupants, "moisture");
    RequireTerrainEnum_<TerrainFeature_t>(rOccupants, "terrain feature");

    // Named in C++ rather than by a terrain enum. A missing entry cannot be placed, and the
    // caller has no later diagnostic: spread would no-op, and fungus placement would throw
    // at the first patch.
    RequireOccupant_(rOccupants, ImprovementIds::k_Fungus, OccupantPlacement_t::Terrain,
                     "fungus placement");
    for (const std::string_view id : {ImprovementIds::k_Forest, ImprovementIds::k_KelpFarm})
    {
        RequireOccupant_(rOccupants, id, OccupantPlacement_t::Improvement, "terraform spread");
    }
}

} // namespace ac
