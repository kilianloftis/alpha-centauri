#include "game/worldgen/WorldGenDecorationConfigParser.h"
#include "lib/config/JsonConfigLoader.h"

#include <optional>
#include <string>

namespace ac
{

namespace
{

MoistureDecorationConfig_t ParseMoisture_(const ConfigReader& rReader)
{
    MoistureDecorationConfig_t config;
    config.cloudmassPeaks = rReader.IntInRange("cloudmass_peaks", 0, 20);
    config.cloudmassHills = rReader.IntInRange("cloudmass_hills", 0, 20);
    config.rainfallCoeff = rReader.IntInRange("rainfall_coeff", 0, 8);
    config.hillMinElevationMeters = rReader.IntAtLeast("hill_min_elevation_meters", 1);
    config.peakMinElevationMeters = rReader.IntAtLeast("peak_min_elevation_meters", 0);
    if (config.peakMinElevationMeters <= config.hillMinElevationMeters)
    {
        rReader.FailKey("peak_min_elevation_meters", "must be > hill_min_elevation_meters");
    }
    return config;
}

ErosiveLevelConfig_t ParseErosiveLevel_(const ConfigReader& rReader)
{
    ErosiveLevelConfig_t level;
    level.landRelief = static_cast<float>(rReader.NumberAbove("land_relief", 0.0));
    level.roughness = static_cast<float>(rReader.NumberAtLeast("roughness", 0.0));
    return level;
}

RockinessWeights_t ParseRockinessWeights_(const ConfigReader& rReader)
{
    RockinessWeights_t weights;
    weights.flat = static_cast<float>(rReader.NumberAtLeast("flat", 0.0));
    weights.rolling = static_cast<float>(rReader.NumberAtLeast("rolling", 0.0));
    weights.rocky = static_cast<float>(rReader.NumberAtLeast("rocky", 0.0));

    const float total = weights.flat + weights.rolling + weights.rocky;
    if (total <= 0.0f)
    {
        rReader.Fail("weights must sum to > 0");
    }

    weights.flat /= total;
    weights.rolling /= total;
    weights.rocky /= total;
    return weights;
}

// Every band but the last has a strictly increasing ceiling; the last covers the rest.
std::optional<int> ParseBandCeiling_(const ConfigReader& rBand, bool bLast,
                                     std::optional<int> previousMax)
{
    const char* pKey = "max_elevation_above_ocean_meters";
    if (bLast)
    {
        if (rBand.Has(pKey))
        {
            rBand.FailKey(pKey, "must be omitted on the last entry");
        }
        return std::nullopt;
    }
    if (!rBand.Has(pKey))
    {
        rBand.FailKey(pKey, "is required on every non-final band");
    }
    const int maxElevation = rBand.Int(pKey);
    if (previousMax.has_value() && maxElevation <= *previousMax)
    {
        rBand.FailKey(pKey, "must be strictly increasing");
    }
    return maxElevation;
}

RockinessDecorationConfig_t ParseRockiness_(const ConfigReader& rReader)
{
    RockinessDecorationConfig_t config;
    config.poles = ParseRockinessWeights_(rReader.Object("poles"));
    config.bands.clear();

    rReader.NonEmptyArray("bands");
    const std::vector<ConfigReader> bands = rReader.Objects("bands");
    std::optional<int> previousMax;
    for (std::size_t i = 0; i < bands.size(); ++i)
    {
        RockinessBand_t band;
        band.maxElevationAboveOceanMeters =
            ParseBandCeiling_(bands[i], i + 1 == bands.size(), previousMax);
        band.weights = ParseRockinessWeights_(bands[i]);
        previousMax = band.maxElevationAboveOceanMeters;
        config.bands.push_back(band);
    }
    return config;
}

float RequireUnitInterval_(const ConfigReader& rReader, const char* pKey)
{
    return static_cast<float>(rReader.NumberInRange(pKey, 0.0, 1.0));
}

FungusDecorationConfig_t ParseFungus_(const ConfigReader& rReader)
{
    FungusDecorationConfig_t config;
    config.coverage = RequireUnitInterval_(rReader, "coverage");
    config.fill = RequireUnitInterval_(rReader, "fill");
    config.poleRowChance = RequireUnitInterval_(rReader, "pole_row_chance");
    config.clusterScaleTiles = rReader.IntAtLeast("cluster_scale_tiles", 1);
    return config;
}

ErosiveDecorationConfig_t ParseErosive_(const ConfigReader& rReader)
{
    ErosiveDecorationConfig_t config;
    config.low = ParseErosiveLevel_(rReader.Object("low"));
    config.average = ParseErosiveLevel_(rReader.Object("average"));
    config.high = ParseErosiveLevel_(rReader.Object("high"));
    return config;
}

WorldGenDecorationConfig_t ParseDecoration_(const ConfigReader& rReader)
{
    WorldGenDecorationConfig_t config;
    config.moisture = ParseMoisture_(rReader.Object("moisture"));
    config.erosive = ParseErosive_(rReader.Object("erosive"));
    config.rockiness = ParseRockiness_(rReader.Object("rockiness"));
    config.aquifers.fraction = RequireUnitInterval_(rReader.Object("aquifers"), "fraction");
    config.fungus = ParseFungus_(rReader.Object("fungus"));
    config.tileBonuses.fraction =
        RequireUnitInterval_(rReader.Object("tile_bonuses"), "fraction");
    return config;
}

} // namespace

WorldGenDecorationConfig_t WorldGenDecorationConfigParser::ParseConfig(
    const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<WorldGenDecorationConfig_t>(
        configPath, "world gen decoration", ParseDecoration_);
}

} // namespace ac
