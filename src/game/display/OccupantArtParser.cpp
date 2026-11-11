#include "game/display/OccupantArtParser.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/TerrainConfig.h"
#include "game/map/Tile.h"

#include <cstddef>
#include <cstdint>
#include <magic_enum.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
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

const std::vector<std::string> k_ArtKeys = {
    "layer", "variants", "tiles", "yield_rows", "ground",
    "hides", "overhang", "depth_shade", "fill_color",
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
                                        const ConfigReader& rArt)
{
    if (!rList.is_array())
    {
        rArt.Fail("'" + rWhat + "' must be an array of strings");
    }
    std::vector<std::string> paths;
    for (const nlohmann::json& rPath : rList)
    {
        if (!rPath.is_string())
        {
            rArt.Fail("'" + rWhat + "' entries must be strings");
        }
        paths.push_back(rPath.get<std::string>());
    }
    return paths;
}

OccupantSpritePaths_t ParseSurfacePaths_(const nlohmann::json& rJson, const std::string& rWhat,
                                         const ConfigReader& rArt)
{
    if (!rJson.is_object())
    {
        rArt.Fail("'" + rWhat + "' must be an object of 'land' and/or 'sea' path lists");
    }
    OccupantSpritePaths_t paths;
    for (const auto& [surface, rList] : rJson.items())
    {
        const auto domain = magic_enum::enum_cast<ImprovementDomain_t>(
            surface, magic_enum::case_insensitive);
        if (!domain)
        {
            rArt.Fail("'" + rWhat + "' has unknown surface '" + surface + "'");
        }
        (*domain == ImprovementDomain_t::Land ? paths.land : paths.sea) =
            ParsePathList_(rList, rWhat + "." + surface, rArt);
    }
    return paths;
}

SpriteTileLayout_t ParseTileLayout_(const nlohmann::json& rJson, const ConfigReader& rArt)
{
    if (!rJson.contains("layout"))
    {
        rArt.Fail("'tiles' needs a 'layout'");
    }
    const nlohmann::json& rValue = rJson.at("layout");
    const auto layout = rValue.is_string()
                            ? magic_enum::enum_cast<SpriteTileLayout_t>(
                                  rValue.get<std::string>(), magic_enum::case_insensitive)
                            : std::nullopt;
    if (!layout)
    {
        rArt.Fail("'tiles' 'layout' must be \"edges\", \"blob\" or \"links\"");
    }
    return *layout;
}

// A surface's path pattern, or empty when the surface has none.
std::string ParseMaskPattern_(const nlohmann::json& rJson, const char* pSurface,
                              const ConfigReader& rArt)
{
    if (!rJson.contains(pSurface))
    {
        return {};
    }
    const nlohmann::json& rValue = rJson.at(pSurface);
    if (!rValue.is_string() || rValue.get<std::string>().find(k_MaskToken) == std::string::npos)
    {
        rArt.Fail("'tiles' '" + std::string(pSurface)
                     + "' must be a path pattern containing {mask}");
    }
    return rValue.get<std::string>();
}

void ParseLinkFields_(const nlohmann::json& rJson, OccupantTileSet_t& rTiles,
                      const ConfigReader& rArt)
{
    if (rJson.contains("link_occupants"))
    {
        rTiles.linkOccupants = rArt.Object("tiles").StringArray("link_occupants");
    }
    if (rJson.contains("replaces_links_of"))
    {
        if (!rJson.at("replaces_links_of").is_string())
        {
            rArt.Fail("'tiles' 'replaces_links_of' must be an occupant id");
        }
        rTiles.replacesLinksOf = rJson.at("replaces_links_of").get<std::string>();
    }
    const bool bLinks = rTiles.layout == SpriteTileLayout_t::Links;
    if (bLinks && rTiles.linkOccupants.empty())
    {
        rArt.Fail("'tiles' with layout \"links\" needs 'link_occupants'");
    }
    if (!bLinks && (!rTiles.linkOccupants.empty() || !rTiles.replacesLinksOf.empty()))
    {
        rArt.Fail("'tiles' names link occupants without layout \"links\"");
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

OccupantTileSet_t ParseTileSet_(const nlohmann::json& rJson, const ConfigReader& rArt)
{
    if (!rJson.is_object())
    {
        rArt.Fail("'tiles' must be an object with a 'layout' and 'land' and/or 'sea' patterns");
    }
    for (const auto& [key, rUnused] : rJson.items())
    {
        if (key != "layout" && key != "link_occupants" && key != "replaces_links_of"
            && key != "land" && key != "sea")
        {
            rArt.Fail("'tiles' has unknown key '" + key + "'");
        }
    }
    OccupantTileSet_t tiles;
    tiles.layout = ParseTileLayout_(rJson, rArt);
    const std::string landPattern = ParseMaskPattern_(rJson, "land", rArt);
    const std::string seaPattern = ParseMaskPattern_(rJson, "sea", rArt);
    if (landPattern.empty() && seaPattern.empty())
    {
        rArt.Fail("'tiles' needs a 'land' or 'sea' pattern");
    }
    ParseLinkFields_(rJson, tiles, rArt);
    const std::size_t maskCount = MaskCount_(tiles.layout);
    tiles.paths.land = ExpandMaskPattern_(landPattern, maskCount);
    tiles.paths.sea = ExpandMaskPattern_(seaPattern, maskCount);
    return tiles;
}

OccupantYieldRows_t ParseYieldRows_(const nlohmann::json& rJson, const ConfigReader& rArt)
{
    if (!rJson.is_object() || !rJson.contains("stat") || !rJson.at("stat").is_string())
    {
        rArt.Fail("'yield_rows' must be an object with a 'stat' and 'land' and/or 'sea' path "
                  "lists");
    }
    const auto stat = magic_enum::enum_cast<YieldStat_t>(rJson.at("stat").get<std::string>(),
                                                         magic_enum::case_insensitive);
    if (!stat)
    {
        rArt.Fail("'yield_rows' 'stat' must be \"nutrients\", \"minerals\" or \"energy\"");
    }
    nlohmann::json paths = rJson;
    paths.erase("stat");
    OccupantYieldRows_t rows{*stat, ParseSurfacePaths_(paths, "yield_rows", rArt)};
    if (rows.paths.land.empty() && rows.paths.sea.empty())
    {
        rArt.Fail("'yield_rows' needs a 'land' or 'sea' path list");
    }
    return rows;
}

std::unordered_map<std::string, std::vector<std::string>> ParseGround_(
    const nlohmann::json& rJson, const ConfigReader& rArt)
{
    if (!rJson.is_object())
    {
        rArt.Fail("'ground' must be an object of moisture names to path lists");
    }
    std::unordered_map<std::string, std::vector<std::string>> ground;
    for (const auto& [moisture, rList] : rJson.items())
    {
        if (moisture != ToString(Moisture_t::Arid) && moisture != ToString(Moisture_t::Moist)
            && moisture != ToString(Moisture_t::Wet))
        {
            rArt.Fail("'ground' has unknown moisture '" + moisture + "'");
        }
        std::vector<std::string> paths = ParsePathList_(rList, "ground." + moisture, rArt);
        if (paths.empty())
        {
            rArt.Fail("'ground' '" + moisture + "' must be a non-empty array of paths");
        }
        ground.emplace(moisture, std::move(paths));
    }
    return ground;
}

Color_t ParseFillColor_(const nlohmann::json& rJson, const ConfigReader& rArt)
{
    if (!rJson.is_array() || rJson.size() < 3 || rJson.size() > 4)
    {
        rArt.Fail("'fill_color' must be an RGB or RGBA array");
    }
    int channels[4] = {0, 0, 0, 255};
    for (std::size_t i = 0; i < rJson.size(); ++i)
    {
        if (!rJson[i].is_number_integer())
        {
            rArt.Fail("'fill_color' channels must be integers");
        }
        channels[i] = rJson[i].get<int>();
        if (channels[i] < 0 || channels[i] > 255)
        {
            rArt.Fail("'fill_color' channels must be within 0..255");
        }
    }
    return Color_t{static_cast<std::uint8_t>(channels[0]), static_cast<std::uint8_t>(channels[1]),
                   static_cast<std::uint8_t>(channels[2]), static_cast<std::uint8_t>(channels[3])};
}

WaterShadeRange_t ParseDepthShade_(const nlohmann::json& rJson, const ConfigReader& rArt)
{
    if (!rJson.is_object() || !rJson.contains("offset") || !rJson.at("offset").is_number_integer()
        || !rJson.contains("max") || !rJson.at("max").is_number_integer())
    {
        rArt.Fail("'depth_shade' must be an object with integer 'offset' and 'max'");
    }
    const WaterShadeRange_t range{rJson.at("offset").get<int>(), rJson.at("max").get<int>()};
    if (range.max < 0)
    {
        rArt.Fail("'depth_shade' 'max' must not be negative");
    }
    return range;
}

ArtLayer_t ParseArtLayer_(const nlohmann::json& rJson, const ConfigReader& rArt)
{
    if (!rJson.contains("layer") || !rJson.at("layer").is_string())
    {
        rArt.Fail("needs a 'layer'");
    }
    const auto layer = magic_enum::enum_cast<ArtLayer_t>(rJson.at("layer").get<std::string>(),
                                                         magic_enum::case_insensitive);
    if (!layer)
    {
        rArt.Fail("'layer' must be landform, moisture, rockiness, landmark, vegetation, river, "
                     "road or object");
    }
    return *layer;
}

// Exactly one sprite shape: plain variants, a mask tile set, or yield rows (objects only).
decltype(OccupantArt_t::sprites) ParseSprites_(const nlohmann::json& rJson, ArtLayer_t layer,
                                               const ConfigReader& rArt)
{
    const int spriteKinds = static_cast<int>(rJson.contains("variants"))
                            + static_cast<int>(rJson.contains("tiles"))
                            + static_cast<int>(rJson.contains("yield_rows"));
    if (spriteKinds != 1)
    {
        rArt.Fail("needs exactly one of 'variants', 'tiles' or 'yield_rows'");
    }
    if (rJson.contains("variants"))
    {
        OccupantSpritePaths_t variants =
            ParseSurfacePaths_(rJson.at("variants"), "variants", rArt);
        if (variants.land.empty() && variants.sea.empty())
        {
            rArt.Fail("'variants' needs a 'land' or 'sea' path list");
        }
        return variants;
    }
    if (rJson.contains("tiles"))
    {
        return ParseTileSet_(rJson.at("tiles"), rArt);
    }
    if (layer != ArtLayer_t::Object)
    {
        rArt.Fail("'yield_rows' is allowed only on the object layer");
    }
    return ParseYieldRows_(rJson.at("yield_rows"), rArt);
}

// Links tile sets belong to roads and roads need them; objects take no tile set.
void RequireSpritesFitLayer_(const OccupantArt_t& rArt, const ConfigReader& rArtReader)
{
    const OccupantTileSet_t* pTiles = std::get_if<OccupantTileSet_t>(&rArt.sprites);
    const bool bLinks = pTiles && pTiles->layout == SpriteTileLayout_t::Links;
    if (bLinks && rArt.layer != ArtLayer_t::Road)
    {
        rArtReader.Fail("'tiles' layout \"links\" is allowed only on the road layer");
    }
    if (rArt.layer == ArtLayer_t::Road && !bLinks)
    {
        rArtReader.Fail("on the road layer needs 'tiles' with layout \"links\"");
    }
    if (rArt.layer == ArtLayer_t::Object && pTiles)
    {
        rArtReader.Fail("'tiles' is not allowed on the object layer");
    }
}

float ParseOverhang_(const ConfigReader& rArt, ArtLayer_t layer)
{
    if (layer != ArtLayer_t::Object)
    {
        rArt.Fail("'overhang' is allowed only on the object layer");
    }
    return static_cast<float>(rArt.NumberAtLeast("overhang", 0.0));
}

} // namespace

std::optional<OccupantArt_t> ParseOccupantArt(const ConfigReader& rEntry)
{
    if (!rEntry.Has("art"))
    {
        return std::nullopt;
    }
    const ConfigReader artReader = rEntry.Object("art");
    const nlohmann::json& rJson = artReader.Json();
    artReader.RejectUnknownKeys(k_ArtKeys);

    OccupantArt_t art;
    art.layer = ParseArtLayer_(rJson, artReader);
    art.sprites = ParseSprites_(rJson, art.layer, artReader);
    RequireSpritesFitLayer_(art, artReader);
    if (rJson.contains("ground"))
    {
        art.ground = ParseGround_(rJson.at("ground"), artReader);
    }
    art.hides = artReader.StringArray("hides");
    if (rJson.contains("overhang"))
    {
        art.overhang = ParseOverhang_(artReader, art.layer);
    }
    if (rJson.contains("depth_shade"))
    {
        if (art.layer != ArtLayer_t::Landform)
        {
            artReader.Fail("'depth_shade' is allowed only on the landform layer");
        }
        art.depthShade = ParseDepthShade_(rJson.at("depth_shade"), artReader);
    }
    if (rJson.contains("fill_color"))
    {
        art.fillColor = ParseFillColor_(rJson.at("fill_color"), artReader);
    }
    return art;
}

} // namespace ac
