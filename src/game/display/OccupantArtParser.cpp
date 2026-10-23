#include "game/display/OccupantArtParser.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/TerrainConfig.h"
#include "game/map/Tile.h"
#include "lib/config/ConfigFields.h"

#include <cstddef>
#include <cstdint>
#include <magic_enum.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace ac
{

namespace
{

constexpr std::string_view k_MaskToken = "{mask}";

constexpr std::size_t k_EdgesMaskCount = 16;
constexpr std::size_t k_BlobMaskCount = 256;
constexpr std::size_t k_LinksMaskCount = 9;

const std::unordered_set<std::string> k_ArtKeys = {
    "layer", "variants", "tiles", "yield_rows", "ground",
    "hides", "overhang", "depth_shade", "fill_color",
};

class ArtErrors
{
public:
    explicit ArtErrors(const std::string& rId)
        : m_id(rId)
    {
    }

    [[noreturn]] void Fail(const std::string& rMessage) const
    {
        throw std::runtime_error("Improvement '" + m_id + "': 'art' " + rMessage);
    }

private:
    const std::string& m_id;
};

std::size_t MaskCount_(SpriteTileLayout_t layout)
{
    switch (layout)
    {
        case SpriteTileLayout_t::Edges:
            return k_EdgesMaskCount;
        case SpriteTileLayout_t::Blob:
            return k_BlobMaskCount;
        case SpriteTileLayout_t::Links:
            return k_LinksMaskCount;
    }
    throw std::runtime_error("MaskCount_: unhandled SpriteTileLayout_t");
}

std::vector<std::string> ParsePathList_(const nlohmann::json& rList, const std::string& rWhat,
                                        const ArtErrors& rErrors)
{
    if (!rList.is_array())
    {
        rErrors.Fail("'" + rWhat + "' must be an array of strings");
    }
    std::vector<std::string> paths;
    for (const nlohmann::json& rPath : rList)
    {
        if (!rPath.is_string())
        {
            rErrors.Fail("'" + rWhat + "' entries must be strings");
        }
        paths.push_back(rPath.get<std::string>());
    }
    return paths;
}

OccupantSpritePaths_t ParseSurfacePaths_(const nlohmann::json& rJson, const std::string& rWhat,
                                         const ArtErrors& rErrors)
{
    if (!rJson.is_object())
    {
        rErrors.Fail("'" + rWhat + "' must be an object of 'land' and/or 'sea' path lists");
    }
    OccupantSpritePaths_t paths;
    for (const auto& [surface, rList] : rJson.items())
    {
        const auto domain = magic_enum::enum_cast<ImprovementDomain_t>(
            surface, magic_enum::case_insensitive);
        if (!domain)
        {
            rErrors.Fail("'" + rWhat + "' has unknown surface '" + surface + "'");
        }
        (*domain == ImprovementDomain_t::Land ? paths.land : paths.sea) =
            ParsePathList_(rList, rWhat + "." + surface, rErrors);
    }
    return paths;
}

SpriteTileLayout_t ParseTileLayout_(const nlohmann::json& rJson, const ArtErrors& rErrors)
{
    if (!rJson.contains("layout"))
    {
        rErrors.Fail("'tiles' needs a 'layout'");
    }
    const nlohmann::json& rValue = rJson.at("layout");
    const auto layout = rValue.is_string()
                            ? magic_enum::enum_cast<SpriteTileLayout_t>(
                                  rValue.get<std::string>(), magic_enum::case_insensitive)
                            : std::nullopt;
    if (!layout)
    {
        rErrors.Fail("'tiles' 'layout' must be \"edges\", \"blob\" or \"links\"");
    }
    return *layout;
}

// A surface's path pattern, or empty when the surface has none.
std::string ParseMaskPattern_(const nlohmann::json& rJson, const char* pSurface,
                              const ArtErrors& rErrors)
{
    if (!rJson.contains(pSurface))
    {
        return {};
    }
    const nlohmann::json& rValue = rJson.at(pSurface);
    if (!rValue.is_string() || rValue.get<std::string>().find(k_MaskToken) == std::string::npos)
    {
        rErrors.Fail("'tiles' '" + std::string(pSurface)
                     + "' must be a path pattern containing {mask}");
    }
    return rValue.get<std::string>();
}

void ParseLinkFields_(const nlohmann::json& rJson, OccupantTileSet_t& rTiles,
                      const ArtErrors& rErrors)
{
    if (rJson.contains("link_occupants"))
    {
        rTiles.linkOccupants = ConfigFields::ParseStringArray(rJson, "link_occupants");
    }
    if (rJson.contains("replaces_links_of"))
    {
        if (!rJson.at("replaces_links_of").is_string())
        {
            rErrors.Fail("'tiles' 'replaces_links_of' must be an occupant id");
        }
        rTiles.replacesLinksOf = rJson.at("replaces_links_of").get<std::string>();
    }
    const bool bLinks = rTiles.layout == SpriteTileLayout_t::Links;
    if (bLinks && rTiles.linkOccupants.empty())
    {
        rErrors.Fail("'tiles' with layout \"links\" needs 'link_occupants'");
    }
    if (!bLinks && (!rTiles.linkOccupants.empty() || !rTiles.replacesLinksOf.empty()))
    {
        rErrors.Fail("'tiles' names link occupants without layout \"links\"");
    }
}

// One path per mask index, {mask} replaced by the index.
std::vector<std::string> ExpandMaskPattern_(const std::string& rPattern, std::size_t maskCount)
{
    std::vector<std::string> paths;
    if (rPattern.empty())
    {
        return paths;
    }
    const std::size_t at = rPattern.find(k_MaskToken);
    for (std::size_t mask = 0; mask < maskCount; ++mask)
    {
        std::string path = rPattern;
        path.replace(at, k_MaskToken.size(), std::to_string(mask));
        paths.push_back(std::move(path));
    }
    return paths;
}

OccupantTileSet_t ParseTileSet_(const nlohmann::json& rJson, const ArtErrors& rErrors)
{
    if (!rJson.is_object())
    {
        rErrors.Fail("'tiles' must be an object with a 'layout' and 'land' and/or 'sea' patterns");
    }
    for (const auto& [key, rUnused] : rJson.items())
    {
        if (key != "layout" && key != "link_occupants" && key != "replaces_links_of"
            && key != "land" && key != "sea")
        {
            rErrors.Fail("'tiles' has unknown key '" + key + "'");
        }
    }
    OccupantTileSet_t tiles;
    tiles.layout = ParseTileLayout_(rJson, rErrors);
    const std::string landPattern = ParseMaskPattern_(rJson, "land", rErrors);
    const std::string seaPattern = ParseMaskPattern_(rJson, "sea", rErrors);
    if (landPattern.empty() && seaPattern.empty())
    {
        rErrors.Fail("'tiles' needs a 'land' or 'sea' pattern");
    }
    ParseLinkFields_(rJson, tiles, rErrors);
    const std::size_t maskCount = MaskCount_(tiles.layout);
    tiles.paths.land = ExpandMaskPattern_(landPattern, maskCount);
    tiles.paths.sea = ExpandMaskPattern_(seaPattern, maskCount);
    return tiles;
}

OccupantYieldRows_t ParseYieldRows_(const nlohmann::json& rJson, const ArtErrors& rErrors)
{
    if (!rJson.is_object() || !rJson.contains("stat") || !rJson.at("stat").is_string())
    {
        rErrors.Fail("'yield_rows' must be an object with a 'stat' and 'land' and/or 'sea' path lists");
    }
    const auto stat = magic_enum::enum_cast<YieldStat_t>(rJson.at("stat").get<std::string>(),
                                                         magic_enum::case_insensitive);
    if (!stat)
    {
        rErrors.Fail("'yield_rows' 'stat' must be \"nutrients\", \"minerals\" or \"energy\"");
    }
    nlohmann::json paths = rJson;
    paths.erase("stat");
    OccupantYieldRows_t rows{*stat, ParseSurfacePaths_(paths, "yield_rows", rErrors)};
    if (rows.paths.land.empty() && rows.paths.sea.empty())
    {
        rErrors.Fail("'yield_rows' needs a 'land' or 'sea' path list");
    }
    return rows;
}

std::unordered_map<std::string, std::vector<std::string>> ParseGround_(
    const nlohmann::json& rJson, const ArtErrors& rErrors)
{
    if (!rJson.is_object())
    {
        rErrors.Fail("'ground' must be an object of moisture names to path lists");
    }
    std::unordered_map<std::string, std::vector<std::string>> ground;
    for (const auto& [moisture, rList] : rJson.items())
    {
        if (moisture != ToString(Moisture_t::Arid) && moisture != ToString(Moisture_t::Moist)
            && moisture != ToString(Moisture_t::Wet))
        {
            rErrors.Fail("'ground' has unknown moisture '" + moisture + "'");
        }
        std::vector<std::string> paths = ParsePathList_(rList, "ground." + moisture, rErrors);
        if (paths.empty())
        {
            rErrors.Fail("'ground' '" + moisture + "' must be a non-empty array of paths");
        }
        ground.emplace(moisture, std::move(paths));
    }
    return ground;
}

Color_t ParseFillColor_(const nlohmann::json& rJson, const ArtErrors& rErrors)
{
    if (!rJson.is_array() || rJson.size() < 3 || rJson.size() > 4)
    {
        rErrors.Fail("'fill_color' must be an RGB or RGBA array");
    }
    int channels[4] = {0, 0, 0, 255};
    for (std::size_t i = 0; i < rJson.size(); ++i)
    {
        if (!rJson[i].is_number_integer())
        {
            rErrors.Fail("'fill_color' channels must be integers");
        }
        channels[i] = rJson[i].get<int>();
        if (channels[i] < 0 || channels[i] > 255)
        {
            rErrors.Fail("'fill_color' channels must be within 0..255");
        }
    }
    return Color_t{static_cast<std::uint8_t>(channels[0]), static_cast<std::uint8_t>(channels[1]),
                   static_cast<std::uint8_t>(channels[2]), static_cast<std::uint8_t>(channels[3])};
}

WaterShadeRange_t ParseDepthShade_(const nlohmann::json& rJson, const ArtErrors& rErrors)
{
    if (!rJson.is_object() || !rJson.contains("offset") || !rJson.at("offset").is_number_integer()
        || !rJson.contains("max") || !rJson.at("max").is_number_integer())
    {
        rErrors.Fail("'depth_shade' must be an object with integer 'offset' and 'max'");
    }
    const WaterShadeRange_t range{rJson.at("offset").get<int>(), rJson.at("max").get<int>()};
    if (range.max < 0)
    {
        rErrors.Fail("'depth_shade' 'max' must not be negative");
    }
    return range;
}

void RequireKnownArtKeys_(const nlohmann::json& rJson, const ArtErrors& rErrors)
{
    if (!rJson.is_object())
    {
        rErrors.Fail("must be an object");
    }
    for (const auto& [key, rValue] : rJson.items())
    {
        if (!k_ArtKeys.contains(key))
        {
            rErrors.Fail("has unknown key '" + key + "'");
        }
    }
}

ArtLayer_t ParseArtLayer_(const nlohmann::json& rJson, const ArtErrors& rErrors)
{
    if (!rJson.contains("layer") || !rJson.at("layer").is_string())
    {
        rErrors.Fail("needs a 'layer'");
    }
    const auto layer = magic_enum::enum_cast<ArtLayer_t>(rJson.at("layer").get<std::string>(),
                                                         magic_enum::case_insensitive);
    if (!layer)
    {
        rErrors.Fail("'layer' must be landform, moisture, rockiness, landmark, vegetation, river, "
                     "road or object");
    }
    return *layer;
}

// Exactly one sprite shape: plain variants, a mask tile set, or yield rows (objects only).
decltype(OccupantArt_t::sprites) ParseSprites_(const nlohmann::json& rJson, ArtLayer_t layer,
                                               const ArtErrors& rErrors)
{
    const int spriteKinds = static_cast<int>(rJson.contains("variants"))
                            + static_cast<int>(rJson.contains("tiles"))
                            + static_cast<int>(rJson.contains("yield_rows"));
    if (spriteKinds != 1)
    {
        rErrors.Fail("needs exactly one of 'variants', 'tiles' or 'yield_rows'");
    }
    if (rJson.contains("variants"))
    {
        OccupantSpritePaths_t variants =
            ParseSurfacePaths_(rJson.at("variants"), "variants", rErrors);
        if (variants.land.empty() && variants.sea.empty())
        {
            rErrors.Fail("'variants' needs a 'land' or 'sea' path list");
        }
        return variants;
    }
    if (rJson.contains("tiles"))
    {
        return ParseTileSet_(rJson.at("tiles"), rErrors);
    }
    if (layer != ArtLayer_t::Object)
    {
        rErrors.Fail("'yield_rows' is allowed only on the object layer");
    }
    return ParseYieldRows_(rJson.at("yield_rows"), rErrors);
}

// Links tile sets belong to roads and roads need them; objects take no tile set.
void RequireSpritesFitLayer_(const OccupantArt_t& rArt, const ArtErrors& rErrors)
{
    const OccupantTileSet_t* pTiles = std::get_if<OccupantTileSet_t>(&rArt.sprites);
    const bool bLinks = pTiles && pTiles->layout == SpriteTileLayout_t::Links;
    if (bLinks && rArt.layer != ArtLayer_t::Road)
    {
        rErrors.Fail("'tiles' layout \"links\" is allowed only on the road layer");
    }
    if (rArt.layer == ArtLayer_t::Road && !bLinks)
    {
        rErrors.Fail("on the road layer needs 'tiles' with layout \"links\"");
    }
    if (rArt.layer == ArtLayer_t::Object && pTiles)
    {
        rErrors.Fail("'tiles' is not allowed on the object layer");
    }
}

float ParseOverhang_(const nlohmann::json& rValue, ArtLayer_t layer, const ArtErrors& rErrors)
{
    if (layer != ArtLayer_t::Object)
    {
        rErrors.Fail("'overhang' is allowed only on the object layer");
    }
    if (!rValue.is_number() || !(rValue.get<float>() >= 0.0f))
    {
        rErrors.Fail("'overhang' must not be negative");
    }
    return rValue.get<float>();
}

} // namespace

std::optional<OccupantArt_t> ParseOccupantArt(const nlohmann::json& rEntryJson,
                                              const std::string& rId)
{
    if (!rEntryJson.contains("art"))
    {
        return std::nullopt;
    }
    const nlohmann::json& rJson = rEntryJson.at("art");
    const ArtErrors errors(rId);
    RequireKnownArtKeys_(rJson, errors);

    OccupantArt_t art;
    art.layer = ParseArtLayer_(rJson, errors);
    art.sprites = ParseSprites_(rJson, art.layer, errors);
    RequireSpritesFitLayer_(art, errors);
    if (rJson.contains("ground"))
    {
        art.ground = ParseGround_(rJson.at("ground"), errors);
    }
    art.hides = ConfigFields::ParseStringArray(rJson, "hides");
    if (rJson.contains("overhang"))
    {
        art.overhang = ParseOverhang_(rJson.at("overhang"), art.layer, errors);
    }
    if (rJson.contains("depth_shade"))
    {
        if (art.layer != ArtLayer_t::Landform)
        {
            errors.Fail("'depth_shade' is allowed only on the landform layer");
        }
        art.depthShade = ParseDepthShade_(rJson.at("depth_shade"), errors);
    }
    if (rJson.contains("fill_color"))
    {
        art.fillColor = ParseFillColor_(rJson.at("fill_color"), errors);
    }
    return art;
}

} // namespace ac
