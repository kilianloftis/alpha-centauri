#include "game/map/TileLayerResolver.h"

#include "game/map/ImprovementConfigParser.h"

namespace ac
{

namespace
{

std::optional<std::string> ResolveLandformLayer_(const Tile& rTile)
{
    if (rTile.IsWater())
    {
        return TileLayerContent::k_Water;
    }

    // Rolling is part of the landform layer; rocky terrain is handled by the Rockiness_t layer.
    if (rTile.GetRockiness() == Rockiness_t::Rolling)
    {
        return TileLayerContent::k_Rolling;
    }

    return TileLayerContent::k_Flat;
}

std::optional<std::string> ResolveMoistureLayer_(const Tile& rTile)
{
    switch (rTile.GetMoisture())
    {
        case Moisture_t::Wet:
            return TileLayerContent::k_Wet;
        case Moisture_t::Moist:
            return TileLayerContent::k_Moist;
        case Moisture_t::Arid:
        default:
            return TileLayerContent::k_Arid;
    }
}

std::optional<std::string> ResolveRockinessLayer_(const Tile& rTile)
{
    if (rTile.GetRockiness() == Rockiness_t::Rocky)
    {
        return TileLayerContent::k_Rocky;
    }

    return std::nullopt;
}

std::optional<std::string> ResolveVegetationLayer_(const Tile& rTile)
{
    // TODO: Define vegetation placement rules and mutual exclusivity (farm vs forest).
    // Boreholes and bases should exclude this layer via placement rules, not here.
    if (rTile.HasImprovement(TileLayerContent::k_Farm))
    {
        return TileLayerContent::k_Farm;
    }

    if (rTile.HasImprovement(TileLayerContent::k_Forest))
    {
        return TileLayerContent::k_Forest;
    }

    return std::nullopt;
}

std::optional<std::string> ResolveRoadLayer_(const Tile& rTile)
{
    if (rTile.HasImprovement(TileLayerContent::k_Road))
    {
        return TileLayerContent::k_Road;
    }

    return std::nullopt;
}

std::optional<std::string> ResolveImprovementLayer_(const Tile& rTile)
{
    // TODO: Define improvement rendering priority, exclusion rules, and monolith/landmark handling.
    // This layer is intended for the single most visually dominant non-road, non-vegetation
    // improvement on the tile (e.g., Borehole, Solar Collector, Monolith).
    for (const ImprovementConfig_t* pImprovement : rTile.GetImprovements())
    {
        const std::string& improvementId = pImprovement->id;
        if (improvementId == TileLayerContent::k_Farm
            || improvementId == TileLayerContent::k_Forest
            || improvementId == TileLayerContent::k_Road)
        {
            continue;
        }

        return improvementId;
    }

    return std::nullopt;
}

} // namespace

std::array<TileLayer_t, k_TileLayerCount> ResolveTileLayers(const Tile& rTile)
{
    return std::array<TileLayer_t, k_TileLayerCount>{
        TileLayer_t(TileLayerType_t::Landform, ResolveLandformLayer_(rTile)),
        TileLayer_t(TileLayerType_t::Moisture, ResolveMoistureLayer_(rTile)),
        TileLayer_t(TileLayerType_t::Rockiness, ResolveRockinessLayer_(rTile)),
        TileLayer_t(TileLayerType_t::Vegetation, ResolveVegetationLayer_(rTile)),
        TileLayer_t(TileLayerType_t::Road, ResolveRoadLayer_(rTile)),
        TileLayer_t(TileLayerType_t::Improvement, ResolveImprovementLayer_(rTile))
    };
}

} // namespace ac
