#include "game/worldgen/LandmarkConfigParser.h"
#include "lib/config/EnumNames.h"
#include "lib/config/JsonConfigLoader.h"
#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace ac
{

namespace
{

LandmarkSculpt_t ParseSculpt_(const ConfigReader& rShape)
{
    LandmarkSculpt_t sculpt;
    if (!rShape.Has("sculpt"))
    {
        return sculpt;
    }

    const ConfigReader reader = rShape.Object("sculpt");
    sculpt.peakElevation = reader.IntOr("peak_elevation", sculpt.peakElevation);
    sculpt.baseElevation = reader.IntOr("base_elevation", sculpt.baseElevation);
    if (reader.Has("rocky_core_radius"))
    {
        sculpt.rockyCoreRadius = static_cast<float>(reader.NumberAtLeast("rocky_core_radius", 0.0));
    }
    if (sculpt.peakElevation < sculpt.baseElevation)
    {
        reader.FailKey("peak_elevation", "must be >= base_elevation");
    }
    return sculpt;
}

LandmarkShape_t ParseDiskShape_(const ConfigReader& rReader)
{
    LandmarkShape_t shape;
    shape.kind = LandmarkShapeKind_t::Disk;
    shape.radius = rReader.IntAtLeast("radius", 0);
    return shape;
}

LandmarkShape_t ParseRingShape_(const ConfigReader& rReader)
{
    LandmarkShape_t shape;
    shape.kind = LandmarkShapeKind_t::Ring;
    shape.outerRadius = rReader.IntAtLeast("outer_radius", 1);
    shape.innerRadius = rReader.Has("inner_radius") ? rReader.IntAtLeast("inner_radius", 0) : 0;
    if (shape.innerRadius >= shape.outerRadius)
    {
        rReader.FailKey("inner_radius", "must be < outer_radius");
    }
    return shape;
}

LandmarkShape_t ParseMaskShape_(const ConfigReader& rReader)
{
    LandmarkShape_t shape;
    shape.kind = LandmarkShapeKind_t::Mask;
    rReader.NonEmptyArray("rows");
    shape.maskRows = rReader.StringArray("rows");
    // All-blank rows expand to no footprint, which placement can only skip in silence.
    const bool bHasCell = std::any_of(
        shape.maskRows.begin(), shape.maskRows.end(), [](const std::string& rRow) {
            return rRow.find_first_of("Xx") != std::string::npos;
        });
    if (!bHasCell)
    {
        rReader.FailKey("rows", "contain no 'X' footprint cell");
    }
    return shape;
}

LandmarkShape_t ParseSculptorShape_(const ConfigReader& rReader)
{
    LandmarkShape_t shape;
    shape.kind = LandmarkShapeKind_t::Sculptor;
    shape.sculptorId = rReader.NonEmptyString("id");
    shape.radius = rReader.Has("radius") ? rReader.IntAtLeast("radius", 1) : 4;
    shape.sculpt = ParseSculpt_(rReader);
    return shape;
}

LandmarkShape_t ParseShape_(const ConfigReader& rReader)
{
    const std::string kind = ToLowerAscii(rReader.String("kind"));
    if (kind == "disk")
    {
        return ParseDiskShape_(rReader);
    }
    if (kind == "ring")
    {
        return ParseRingShape_(rReader);
    }
    if (kind == "mask")
    {
        return ParseMaskShape_(rReader);
    }
    if (kind == "sculptor")
    {
        return ParseSculptorShape_(rReader);
    }
    rReader.FailKey("kind", "is not a known shape kind: '" + kind + "'");
}

LandmarkConfig_t ParseLandmark_(const ConfigReader& rReader)
{
    LandmarkConfig_t config;
    config.id = rReader.NonEmptyString("id");
    config.improvementId = rReader.Has("improvement_id") ? rReader.NonEmptyString("improvement_id")
                                                         : config.id;
    config.domain = rReader.Has("domain")
                        ? rReader.Enum<LandmarkDomain_t>("domain", "landmark domain")
                        : LandmarkDomain_t::Land;
    config.maxCount = rReader.Has("max_count") ? rReader.IntAtLeast("max_count", 0) : 1;
    config.minSpacing = rReader.Has("min_spacing") ? rReader.IntAtLeast("min_spacing", 0) : 16;
    config.setFungus = rReader.BoolOr("set_fungus", false);
    config.shape = ParseShape_(rReader.Object("shape"));
    return config;
}

} // namespace

std::vector<LandmarkConfig_t> LandmarkConfigParser::ParseConfig(
    const std::string& configPath,
    const std::vector<std::string>& rKnownImprovementIds)
{
    auto landmarks =
        JsonConfigLoader::LoadFile<LandmarkConfig_t>(configPath, "landmark", ParseLandmark_);

    if (!rKnownImprovementIds.empty())
    {
        std::unordered_set<std::string> known(rKnownImprovementIds.begin(),
                                              rKnownImprovementIds.end());
        for (const LandmarkConfig_t& rLandmark : landmarks)
        {
            if (known.count(rLandmark.improvementId) == 0)
            {
                throw std::runtime_error(
                    "Landmark '" + rLandmark.id + "' improvement_id '"
                    + rLandmark.improvementId + "' is not a known improvement");
            }
        }
    }

    return landmarks;
}

void LandmarkConfigParser::ValidateSculptAgainstPreset(const LandmarkConfig_t& rLandmark,
                                                       const WorldGenPresetConfig_t& rPreset)
{
    if (rLandmark.shape.kind != LandmarkShapeKind_t::Sculptor)
    {
        return;
    }

    const LandmarkSculpt_t& rSculpt = rLandmark.shape.sculpt;
    if (rSculpt.baseElevation < rPreset.minElevation || rSculpt.peakElevation > rPreset.maxElevation)
    {
        throw std::runtime_error(
            "Landmark '" + rLandmark.id + "' sculpt elevations ["
            + std::to_string(rSculpt.baseElevation) + ", "
            + std::to_string(rSculpt.peakElevation) + "] are outside preset '"
            + rPreset.id + "' [" + std::to_string(rPreset.minElevation) + ", "
            + std::to_string(rPreset.maxElevation) + "]");
    }
}

} // namespace ac
