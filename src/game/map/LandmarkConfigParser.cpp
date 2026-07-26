#include "game/map/LandmarkConfigParser.h"

#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <unordered_set>

namespace ac
{

namespace
{

std::string ToLower_(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

LandmarkDomain_t ParseDomain_(const std::string& value)
{
    const std::string normalized = ToLower_(value);
    if (normalized == "land")
    {
        return LandmarkDomain_t::Land;
    }
    if (normalized == "water")
    {
        return LandmarkDomain_t::Water;
    }
    if (normalized == "any")
    {
        return LandmarkDomain_t::Any;
    }
    throw std::runtime_error("Unknown landmark domain: '" + value + "'");
}

LandmarkShape_t ParseShape_(const nlohmann::json& rJson, const std::string& landmarkId)
{
    if (!rJson.is_object() || !rJson.contains("kind"))
    {
        throw std::runtime_error("Landmark '" + landmarkId + "' shape requires 'kind'");
    }

    LandmarkShape_t shape;
    const std::string kind = ToLower_(rJson.at("kind").get<std::string>());
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
    config.domain = ParseDomain_(rJson.value("domain", "land"));
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
