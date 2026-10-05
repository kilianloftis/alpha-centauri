#include "game/map/TileLayerResolver.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementIds.h"

#include <algorithm>
#include <stdexcept>

namespace ac
{

namespace
{

std::optional<std::string> ResolveLandformLayer_(const Tile& rTile)
{
    if (rTile.IsWater())
    {
        // Depth bands are PascalCase config ids (Ocean / OceanShelf); shared Water is fallback.
        if (rTile.HasFeature("OceanShelf"))
        {
            return std::string("OceanShelf");
        }
        if (rTile.HasFeature("Ocean"))
        {
            return std::string("Ocean");
        }
        return TileLayerContent::k_Water;
    }

    // Flat land has no dedicated sheet art — moisture bases carry the tile. Rolling/rocky
    // are overlays on the rockiness layer so they draw above rainfall.
    return std::nullopt;
}

std::optional<std::string> ResolveMoistureLayer_(const Tile& rTile)
{
    // Sea tiles keep moisture for yields/effects, but land rainfall art must not cover water.
    if (rTile.IsWater())
    {
        return std::nullopt;
    }
    switch (rTile.GetMoisture())
    {
        case Moisture_t::Wet:
            return TileLayerContent::k_Wet;
        case Moisture_t::Moist:
            return TileLayerContent::k_Moist;
        case Moisture_t::Arid:
            return TileLayerContent::k_Arid;
    }
    throw std::runtime_error("ResolveMoistureLayer_: unhandled Moisture_t");
}

std::optional<std::string> ResolveRockinessLayer_(const Tile& rTile)
{
    if (rTile.IsWater())
    {
        return std::nullopt;
    }
    switch (rTile.GetRockiness())
    {
        case Rockiness_t::Rocky:
            return TileLayerContent::k_Rocky;
        case Rockiness_t::Rolling:
            return TileLayerContent::k_Rolling;
        case Rockiness_t::Flat:
            return std::nullopt;
    }
    throw std::runtime_error("ResolveRockinessLayer_: unhandled Rockiness_t");
}

// Landmarks exclude each other, so a tile has at most one. Like the Improvement layer, this
// returns the config id.
std::optional<std::string> ResolveLandmarkLayer_(const Tile& rTile)
{
    for (const ImprovementConfig_t* pFeature : rTile.GetTerrainFeatures())
    {
        if (pFeature && std::ranges::find(pFeature->tags, "landmark") != pFeature->tags.end())
        {
            return pFeature->id;
        }
    }
    return std::nullopt;
}

// A river's last tile can be water; like SMAC, river art draws on land only.
std::optional<std::string> ResolveRiverLayer_(const Tile& rTile)
{
    if (rTile.IsLand() && rTile.GetHasRiver())
    {
        return TileLayerContent::k_River;
    }
    return std::nullopt;
}

std::optional<std::string> ResolveVegetationLayer_(const Tile& rTile)
{
    // Fungus replaces farm/forest visually (Forest excludes Fungus in config; either may win).
    // Probe with config ids, return sprite content ids: the two domains differ in case.
    if (rTile.HasFeature(ImprovementIds::k_Fungus))
    {
        return TileLayerContent::k_Fungus;
    }

    if (rTile.HasImprovement(ImprovementIds::k_Farm))
    {
        return TileLayerContent::k_Farm;
    }

    if (rTile.HasImprovement(ImprovementIds::k_Forest))
    {
        return TileLayerContent::k_Forest;
    }

    return std::nullopt;
}

std::optional<std::string> ResolveRoadLayer_(const Tile& rTile)
{
    if (rTile.HasImprovement(ImprovementIds::k_Road))
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
        if (improvementId == ImprovementIds::k_Farm || improvementId == ImprovementIds::k_Forest
            || improvementId == ImprovementIds::k_Road)
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
        TileLayer_t(TileLayerType_t::Landmark, ResolveLandmarkLayer_(rTile)),
        TileLayer_t(TileLayerType_t::Vegetation, ResolveVegetationLayer_(rTile)),
        TileLayer_t(TileLayerType_t::River, ResolveRiverLayer_(rTile)),
        TileLayer_t(TileLayerType_t::Road, ResolveRoadLayer_(rTile)),
        TileLayer_t(TileLayerType_t::Improvement, ResolveImprovementLayer_(rTile))
    };
}

} // namespace ac
