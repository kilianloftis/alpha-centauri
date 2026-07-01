#include "game/map/TileLayerResolver.h"

#include "game/map/ImprovementConfigParser.h"

namespace ac
{

namespace
{

std::optional<std::string> ResolveLandformLayer_(const Tile& rTile)
{
    // TODO: Define formal water threshold and landform generation rules.
    if (rTile.GetElevation() < 0)
    {
        return TileLayerContent::k_water;
    }

    // Rolling is part of the landform layer; rocky terrain is handled by the Rockiness layer.
    if (rTile.GetRockiness() == Rockiness::Rolling)
    {
        return TileLayerContent::k_rolling;
    }

    return TileLayerContent::k_flat;
}

std::optional<std::string> ResolveMoistureLayer_(const Tile& rTile)
{
    switch (rTile.GetMoisture())
    {
        case Moisture::Wet:
            return TileLayerContent::k_wet;
        case Moisture::Moist:
            return TileLayerContent::k_moist;
        case Moisture::Arid:
        default:
            return TileLayerContent::k_arid;
    }
}

std::optional<std::string> ResolveRockinessLayer_(const Tile& rTile)
{
    if (rTile.GetRockiness() == Rockiness::Rocky)
    {
        return TileLayerContent::k_rocky;
    }

    return std::nullopt;
}

std::optional<std::string> ResolveVegetationLayer_(const Tile& rTile)
{
    // TODO: Define vegetation placement rules and mutual exclusivity (farm vs forest).
    // Boreholes and bases should exclude this layer via placement rules, not here.
    if (rTile.HasImprovement(TileLayerContent::k_farm))
    {
        return TileLayerContent::k_farm;
    }

    if (rTile.HasImprovement(TileLayerContent::k_forest))
    {
        return TileLayerContent::k_forest;
    }

    return std::nullopt;
}

std::optional<std::string> ResolveRoadLayer_(const Tile& rTile)
{
    if (rTile.HasImprovement(TileLayerContent::k_road))
    {
        return TileLayerContent::k_road;
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
        if (improvementId == TileLayerContent::k_farm
            || improvementId == TileLayerContent::k_forest
            || improvementId == TileLayerContent::k_road)
        {
            continue;
        }

        return improvementId;
    }

    return std::nullopt;
}

} // namespace

std::array<TileLayer_t, k_tileLayerCount> ResolveTileLayers(const Tile& rTile)
{
    return std::array<TileLayer_t, k_tileLayerCount>{
        TileLayer_t(TileLayerType_t::Landform, ResolveLandformLayer_(rTile)),
        TileLayer_t(TileLayerType_t::Moisture, ResolveMoistureLayer_(rTile)),
        TileLayer_t(TileLayerType_t::Rockiness, ResolveRockinessLayer_(rTile)),
        TileLayer_t(TileLayerType_t::Vegetation, ResolveVegetationLayer_(rTile)),
        TileLayer_t(TileLayerType_t::Road, ResolveRoadLayer_(rTile)),
        TileLayer_t(TileLayerType_t::Improvement, ResolveImprovementLayer_(rTile))
    };
}

} // namespace ac
