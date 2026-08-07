#include "game/map/LandmarkConfigParser.h"

#include "game/map/Tile.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/EnumNames.h"
#include "lib/config/JsonConfigLoader.h"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace ac
{

namespace
{

LandmarkSculpt_t ParseSculpt_(const nlohmann::json& rShapeJson, const std::string& landmarkId)
{
    LandmarkSculpt_t sculpt;
    if (!rShapeJson.contains("sculpt"))
    {
        return sculpt;
    }

    const nlohmann::json& rJson = rShapeJson.at("sculpt");
    if (!rJson.is_object())
    {
        throw std::runtime_error("Landmark '" + landmarkId + "' sculpt must be an object");
    }

    sculpt.peakElevation = rJson.value("peak_elevation", sculpt.peakElevation);
    sculpt.baseElevation = rJson.value("base_elevation", sculpt.baseElevation);
    sculpt.rockyCoreRadius = rJson.value("rocky_core_radius", sculpt.rockyCoreRadius);

    if (sculpt.peakElevation < sculpt.baseElevation)
    {
        throw std::runtime_error("Landmark '" + landmarkId
                                 + "' sculpt peak_elevation must be >= base_elevation");
    }
    if (sculpt.rockyCoreRadius < 0.0f)
    {
        throw std::runtime_error("Landmark '" + landmarkId
                                 + "' sculpt rocky_core_radius must be >= 0");
    }
    if (sculpt.baseElevation < k_MinElevation || sculpt.peakElevation > k_MaxElevation)
    {
        throw std::runtime_error("Landmark '" + landmarkId + "' sculpt elevations ["
                                 + std::to_string(sculpt.baseElevation) + ", "
                                 + std::to_string(sculpt.peakElevation)
                                 + "] are outside Planet's [" + std::to_string(k_MinElevation)
                                 + ", " + std::to_string(k_MaxElevation) + "]");
    }
    return sculpt;
}

LandmarkShape_t ParseShape_(const nlohmann::json& rJson, const std::string& landmarkId)
{
    if (!rJson.is_object() || !rJson.contains("kind"))
    {
        throw std::runtime_error("Landmark '" + landmarkId + "' shape requires 'kind'");
    }

    LandmarkShape_t shape;
    const std::string kind = ToLowerAscii(rJson.at("kind").get<std::string>());
    if (kind == "disk")
    {
        shape.kind = LandmarkShapeKind_t::Disk;
        shape.radius = rJson.at("radius").get<int>();
        if (shape.radius < 0)
        {
            throw std::runtime_error("Landmark '" + landmarkId + "' disk radius must be >= 0");
        }
    }
    else if (kind == "ring")
    {
        shape.kind = LandmarkShapeKind_t::Ring;
        shape.outerRadius = rJson.at("outer_radius").get<int>();
        shape.innerRadius = rJson.value("inner_radius", 0);
        if (shape.outerRadius < 1 || shape.innerRadius < 0
            || shape.innerRadius >= shape.outerRadius)
        {
            throw std::runtime_error(
                "Landmark '" + landmarkId
                + "' ring requires 0 <= inner_radius < outer_radius and outer_radius >= 1");
        }
    }
    else if (kind == "mask")
    {
        shape.kind = LandmarkShapeKind_t::Mask;
        if (!rJson.contains("rows") || !rJson.at("rows").is_array() || rJson.at("rows").empty())
        {
            throw std::runtime_error("Landmark '" + landmarkId + "' mask requires non-empty 'rows'");
        }
        for (const auto& row : rJson.at("rows"))
        {
            shape.maskRows.push_back(row.get<std::string>());
        }
        // All-blank rows expand to no footprint, which placement can only skip in silence.
        const bool bHasCell = std::any_of(
            shape.maskRows.begin(), shape.maskRows.end(), [](const std::string& rRow) {
                return rRow.find_first_of("Xx") != std::string::npos;
            });
        if (!bHasCell)
        {
            throw std::runtime_error("Landmark '" + landmarkId
                                     + "' mask rows contain no 'X' footprint cell");
        }
    }
    else if (kind == "sculptor")
    {
        shape.kind = LandmarkShapeKind_t::Sculptor;
        shape.sculptorId = rJson.at("id").get<std::string>();
        shape.radius = rJson.value("radius", 4);
        if (shape.sculptorId.empty())
        {
            throw std::runtime_error("Landmark '" + landmarkId + "' sculptor requires 'id'");
        }
        if (shape.radius < 1)
        {
            throw std::runtime_error(
                "Landmark '" + landmarkId + "' sculptor radius must be >= 1");
        }
        shape.sculpt = ParseSculpt_(rJson, landmarkId);
    }
    else
    {
        throw std::runtime_error("Landmark '" + landmarkId + "' unknown shape kind '" + kind + "'");
    }
    return shape;
}

LandmarkConfig_t ParseLandmark_(const nlohmann::json& rJson)
{
    LandmarkConfig_t config;
    config.id = ConfigFields::ParseId(rJson);
    config.improvementId = rJson.value("improvement_id", config.id);
    if (config.improvementId.empty())
    {
        throw std::runtime_error("Landmark '" + config.id + "' has empty improvement_id");
    }
    config.domain =
        EnumFromName<LandmarkDomain_t>(rJson.value("domain", "land"), "landmark domain");
    config.maxCount = rJson.value("max_count", 1);
    config.minSpacing = rJson.value("min_spacing", 16);
    config.setFungus = rJson.value("set_fungus", false);
    if (config.maxCount < 0)
    {
        throw std::runtime_error("Landmark '" + config.id + "' max_count must be >= 0");
    }
    if (config.minSpacing < 0)
    {
        throw std::runtime_error("Landmark '" + config.id + "' min_spacing must be >= 0");
    }
    if (!rJson.contains("shape"))
    {
        throw std::runtime_error("Landmark '" + config.id + "' missing required 'shape'");
    }
    config.shape = ParseShape_(rJson.at("shape"), config.id);
    return config;
}

} // namespace

std::vector<LandmarkConfig_t> LandmarkConfigParser::ParseConfig(
    const std::string& configPath,
    const std::vector<std::string>& rKnownImprovementIds)
{
    auto landmarks = JsonConfigLoader::LoadFile<LandmarkConfig_t>(
        configPath, "landmark",
        [](const nlohmann::json& rJson) { return ParseLandmark_(rJson); });

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

} // namespace ac
