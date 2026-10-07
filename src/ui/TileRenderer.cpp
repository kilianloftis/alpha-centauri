#include "ui/TileRenderer.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementIds.h"
#include "game/map/RiverGeneration.h"
#include "game/map/Tile.h"
#include "game/map/TileLayer.h"
#include "game/map/TileLayerResolver.h"
#include "game/map/WorldMap.h"
#include "graphics/Graphics.h"
#include "ui/CoastOverlay.h"
#include "ui/TileAutotile.h"
#include "ui/TileSpriteEdgeInset.h"
#include "ui/WaterShading.h"
#include "ui/style/UiStyle.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ac
{

namespace
{

constexpr float k_IsoHeightRatio = 0.5f;
constexpr std::uint64_t k_FnvOffset = 14695981039346656037ull;
constexpr std::uint64_t k_FnvPrime  = 1099511628211ull;

void MixHash_(std::uint64_t& rHash, std::uint64_t value)
{
    rHash ^= value;
    rHash *= k_FnvPrime;
}

enum class SpriteCacheState_t
{
    Untried,
    Loaded,
    Missing,
};

// Paths already probed this process. Assets are static for a run; avoid re-statting / reloading
// every visible tile every frame.
std::unordered_map<std::string, SpriteCacheState_t>& SpriteCache_()
{
    static std::unordered_map<std::string, SpriteCacheState_t> cache;
    return cache;
}

uint8_t LerpChannel_(uint8_t a, uint8_t b, float t)
{
    return static_cast<uint8_t>(std::lround(static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t));
}

Color_t LerpColor_(const Color_t& a, const Color_t& b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return Color_t{
        LerpChannel_(a.r, b.r, t),
        LerpChannel_(a.g, b.g, t),
        LerpChannel_(a.b, b.b, t),
        LerpChannel_(a.a, b.a, t),
    };
}

Color_t DimColor_(const Color_t& color, float ratio)
{
    ratio = std::clamp(ratio, 0.0f, 1.0f);
    return Color_t{
        static_cast<uint8_t>(std::lround(static_cast<float>(color.r) * ratio)),
        static_cast<uint8_t>(std::lround(static_cast<float>(color.g) * ratio)),
        static_cast<uint8_t>(std::lround(static_cast<float>(color.b) * ratio)),
        color.a,
    };
}

float Remap01_(float value, float inMin, float inMax)
{
    if (inMax <= inMin)
    {
        return 0.0f;
    }
    return (value - inMin) / (inMax - inMin);
}

// TileLayerContent ids are lowercase; ImprovementConfig_t::id values are PascalCase.
std::string ContentIdToConfigId_(const std::string& contentId)
{
    if (contentId.empty())
    {
        return contentId;
    }
    std::string id = contentId;
    id[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(id[0])));
    return id;
}

const ImprovementConfig_t* FindOccupantByConfigId_(const Tile& rTile, std::string_view configId)
{
    for (const ImprovementConfig_t* pFeature : rTile.GetTerrainFeatures())
    {
        if (pFeature && pFeature->id == configId)
        {
            return pFeature;
        }
    }
    for (const ImprovementConfig_t* pImprovement : rTile.GetImprovements())
    {
        if (pImprovement && pImprovement->id == configId)
        {
            return pImprovement;
        }
    }
    return nullptr;
}

// Empty path or missing file → false, so the caller can paint the procedural fallback.
bool EnsureSpriteLoaded_(Graphics& rGraphics, const std::string& path)
{
    if (path.empty())
    {
        return false;
    }

    SpriteCacheState_t& rState = SpriteCache_()[path];
    if (rState == SpriteCacheState_t::Untried)
    {
        rState = std::filesystem::exists(path) && rGraphics.LoadTexture(path, path)
                     ? SpriteCacheState_t::Loaded
                     : SpriteCacheState_t::Missing;
    }
    return rState == SpriteCacheState_t::Loaded;
}

bool TryDrawSprite_(Graphics& rGraphics, const std::string& path, float x, float y, float width,
                    float height, const Color_t& tint)
{
    return EnsureSpriteLoaded_(rGraphics, path)
           && rGraphics.DrawSprite(path, x, y, width, height, tint);
}

// Tile art is palette indices, so it draws only when the palette loads too.
bool TryDrawTileSprite_(Graphics& rGraphics, const std::string& path, const TileShape_t& rShape)
{
    const std::string& palette = Style().tileRenderer.palettePath;
    return EnsureSpriteLoaded_(rGraphics, path) && EnsureSpriteLoaded_(rGraphics, palette)
           && rGraphics.DrawTileSprite(path, palette, rShape);
}

// The shape with every vertex at one shade.
TileShape_t UniformShade_(const TileShape_t& rShape, float shade)
{
    TileShape_t shape = rShape;
    for (TileVertex_t* pVertex : {&shape.center, &shape.west, &shape.north, &shape.east,
                                  &shape.south})
    {
        pVertex->shade = shade;
    }
    return shape;
}

// Land in sight keeps the relief's shades; out of sight SMAC shades land a flat
// fog_land_shade instead of lighting its slopes. Terrain layers on water draw as painted.
TileShape_t TerrainShape_(const TileShape_t& rShape, const Tile& rTile, bool bFogged)
{
    if (!rTile.IsLand())
    {
        return UniformShade_(rShape, 0.0f);
    }
    return bFogged ? UniformShade_(rShape, Style().tileRenderer.fogLandShade) : rShape;
}

// The point (u, v) of the flat tile (0..1 across its box, 0..1 down) carried through the
// shape's four triangles. Each triangle is the centre and two corners; the weights are the
// point's offsets toward those corners.
TileVertex_t PointInShape_(const TileShape_t& rShape, float u, float v)
{
    const float dx = u - 0.5f;
    const float dy = v - 0.5f;
    const auto blend = [&rShape](const TileVertex_t& rA, float wa, const TileVertex_t& rB,
                                 float wb) {
        const TileVertex_t& rC = rShape.center;
        TileVertex_t point = rC;
        point.x = rC.x + wa * (rA.x - rC.x) + wb * (rB.x - rC.x);
        point.y = rC.y + wa * (rA.y - rC.y) + wb * (rB.y - rC.y);
        point.shade = rC.shade + wa * (rA.shade - rC.shade) + wb * (rB.shade - rC.shade);
        return point;
    };
    if (dx <= 0.0f && dy <= 0.0f)
    {
        return blend(rShape.west, -2.0f * dx, rShape.north, -2.0f * dy);
    }
    if (dx >= 0.0f && dy <= 0.0f)
    {
        return blend(rShape.north, -2.0f * dy, rShape.east, 2.0f * dx);
    }
    if (dx >= 0.0f && dy >= 0.0f)
    {
        return blend(rShape.east, 2.0f * dx, rShape.south, 2.0f * dy);
    }
    return blend(rShape.south, 2.0f * dy, rShape.west, -2.0f * dx);
}

// The diamond inscribed in a flat sub-box of the tile (fractions of its box), carried through
// the shape.
TileShape_t SubShape_(const TileShape_t& rShape, float u0, float v0, float uSpan, float vSpan)
{
    const float uMid = u0 + uSpan * 0.5f;
    const float vMid = v0 + vSpan * 0.5f;
    return TileShape_t{
        PointInShape_(rShape, uMid, vMid),
        PointInShape_(rShape, u0, vMid),
        PointInShape_(rShape, uMid, v0),
        PointInShape_(rShape, u0 + uSpan, vMid),
        PointInShape_(rShape, uMid, v0 + vSpan),
    };
}

// The rockiness overlay's inset diamond (DestRectForEdgeInsets in tile fractions).
TileShape_t RockinessShape_(const TileShape_t& rShape, const Tile& rTile, const WorldMap* pMap)
{
    const SpriteDestRect_t dest = DestRectForEdgeInsets(
        0.0f, 0.0f, 1.0f, MatchRockinessEdges(rTile, pMap),
        Style().tileRenderer.spriteOverlayEdgeInsetRatio);
    return SubShape_(rShape, dest.x, dest.y / k_IsoHeightRatio, dest.width,
                     dest.height / k_IsoHeightRatio);
}

void FillInsetShape_(Graphics& rGraphics, const TileShape_t& rShape, float insetRatio,
                     const Color_t& color)
{
    const float span = 1.0f - 2.0f * insetRatio;
    if (span <= 0.0f)
    {
        return;
    }
    rGraphics.FillTileShape(SubShape_(rShape, insetRatio, insetRatio, span, span), color);
}

constexpr char k_CoastCornerNames[k_CoastCornerCount] = {'w', 'n', 'e', 's'};

std::string CoastSpritePath_(std::string_view part, const CoastCornerArt_t& rArt)
{
    std::string path = Style().tileRenderer.coastSpriteDir;
    path += '/';
    path += part;
    path += '_';
    path += k_CoastCornerNames[static_cast<std::size_t>(rArt.corner)];
    path += '_';
    path += std::to_string(rArt.waterMask);
    if (rArt.bAlternate)
    {
        path += "_alt";
    }
    path += ".png";
    return path;
}

// Ocean and shore art baked per diamond corner (extract_terrain.py). The water is shaded by the
// depths around the land tile's own vertices, so it meets the neighboring water's shading at
// their shared corners; the shore draws at shoreShade.
void DrawCoastOverlay_(Graphics& rGraphics, const Tile& rTile, const WorldMap& rMap,
                       const TileShape_t& rShape, float shoreShade)
{
    const CoastOverlay_t overlay = ResolveCoastOverlay(rTile, rMap);
    if (std::ranges::none_of(overlay.corners,
                             [](const CoastCornerArt_t& rArt) { return rArt.waterMask != 0; }))
    {
        return;
    }
    const WaterShadingStyle_t& rShading = Style().tileRenderer.waterShading;
    TileShape_t water = UniformShade_(rShape, 0.0f);
    ApplyWaterShades(water, ResolveWaterShades(rTile, &rMap, rShading),
                     rShading.shades.at(rShading.coastShades));
    for (const CoastCornerArt_t& rArt : overlay.corners)
    {
        if (rArt.waterMask != 0)
        {
            (void)TryDrawTileSprite_(rGraphics, CoastSpritePath_("water", rArt), water);
        }
    }
    const TileShape_t shore = UniformShade_(rShape, shoreShade);
    for (const CoastCornerArt_t& rArt : overlay.corners)
    {
        if (rArt.waterMask != 0)
        {
            (void)TryDrawTileSprite_(rGraphics, CoastSpritePath_("shore", rArt), shore);
        }
    }
}

const std::vector<std::string>& SurfaceSpritePaths_(const ImprovementConfig_t& rOccupant,
                                                    const Tile& rTile)
{
    return rTile.IsWater() ? rOccupant.spritePaths.sea : rOccupant.spritePaths.land;
}

// Object sprites are a ter1.pcx cell: a tile-high footprint plus their overhang ratio. SMAC draws
// the cell from the tile's top corner down, seated at the mean of the tile's four corners rather
// than its raised centre; the art sits high in its cells, so a tile bonus lands mid-tile.
bool TryDrawOccupantPath_(Graphics& rGraphics, const ImprovementConfig_t& rOccupant,
                          const std::string& path, const TileShape_t& rShape, const Color_t& tint)
{
    const float width = rShape.east.x - rShape.west.x;
    const float height = width * k_IsoHeightRatio;
    const float overhang = height * rOccupant.spriteOverhangRatio;
    const float seatX = (rShape.west.x + rShape.north.x + rShape.east.x + rShape.south.x) * 0.25f;
    const float seatY = (rShape.west.y + rShape.north.y + rShape.east.y + rShape.south.y) * 0.25f;
    return TryDrawSprite_(rGraphics, path, seatX - width * 0.5f, seatY - height * 0.5f, width,
                          height + overhang, tint);
}

const std::string& VariantSpritePath_(const ImprovementConfig_t& rOccupant, const Tile& rTile)
{
    return PickSpritePath(SurfaceSpritePaths_(rOccupant, rTile), rTile.GetX(), rTile.GetY(),
                          rOccupant.id);
}

using NeighborRule_t = std::function<bool(const Tile& rNeighbor)>;

// Moisture tiles connect to water and to land at least as wet, fading out toward drier land;
// every other tile set connects to neighbors with the same occupant.
NeighborRule_t LayerNeighborRule_(TileLayerType_t layer, const Tile& rTile,
                                  const ImprovementConfig_t& rOccupant)
{
    if (layer == TileLayerType_t::Moisture)
    {
        return [&rTile](const Tile& rNeighbor) {
            return rNeighbor.IsWater()
                   || static_cast<int>(rNeighbor.GetMoisture())
                          >= static_cast<int>(rTile.GetMoisture());
        };
    }
    return [&rOccupant](const Tile& rNeighbor) { return rNeighbor.HasFeature(rOccupant.id); };
}

constexpr std::string_view k_MaskToken = "{mask}";

std::string TileSpritePath_(const OccupantSpriteTiles_t& rTiles, const Tile& rTile,
                            const WorldMap* pMap, const NeighborRule_t& matches)
{
    std::string path = rTile.IsWater() ? rTiles.sea : rTiles.land;
    if (path.empty())
    {
        return path;
    }
    const std::uint8_t mask = pMap ? ResolveTileMask(rTiles.layout, rTile, *pMap, matches) : 0;
    path.replace(path.find(k_MaskToken), k_MaskToken.size(), std::to_string(mask));
    return path;
}

const ImprovementConfig_t* FindLayerOccupant_(const Tile& rTile, const TileLayer_t& rLayer)
{
    // Water landforms, Landmark and Improvement layers return PascalCase config ids; other
    // layers use TileLayerContent.
    const std::string& contentId = *rLayer.contentId;
    const bool bLooksLikeConfigId =
        !contentId.empty() && std::isupper(static_cast<unsigned char>(contentId.front()));
    const std::string configId = bLooksLikeConfigId ? contentId : ContentIdToConfigId_(contentId);
    const ImprovementConfig_t* pOccupant = rTile.FindOccupantConfig(configId);
    return pOccupant ? pOccupant : FindOccupantByConfigId_(rTile, configId);
}

std::string LayerSpritePath_(const Tile& rTile, const TileLayer_t& rLayer,
                             const ImprovementConfig_t& rOccupant, const WorldMap* pMap)
{
    return rOccupant.spriteTiles
               ? TileSpritePath_(*rOccupant.spriteTiles, rTile, pMap,
                                 LayerNeighborRule_(rLayer.type, rTile, rOccupant))
               : VariantSpritePath_(rOccupant, rTile);
}

// A terrain layer: a baked terrain diamond drawn on the shape.
bool TryDrawLayerSprite_(Graphics& rGraphics, const Tile& rTile, const TileLayer_t& rLayer,
                         const WorldMap* pMap, const TileShape_t& rShape)
{
    const ImprovementConfig_t* pOccupant = FindLayerOccupant_(rTile, rLayer);
    if (!pOccupant)
    {
        return false;
    }
    return TryDrawTileSprite_(rGraphics, LayerSpritePath_(rTile, rLayer, *pOccupant, pMap),
                              rShape);
}

// Water art shaded per vertex by depth within the shade range named by the landform's id; a
// landform without one draws as painted. The deep and shelf landforms trade art by depth, as SMAC
// picks its deep or shelf texture per tile.
bool TryDrawWaterLandform_(Graphics& rGraphics, const Tile& rTile, const TileLayer_t& rLayer,
                           const WorldMap* pMap, const TileShape_t& rShape)
{
    const ImprovementConfig_t* pOccupant = FindLayerOccupant_(rTile, rLayer);
    if (!pOccupant)
    {
        return false;
    }
    const WaterShadingStyle_t& rShading = Style().tileRenderer.waterShading;
    TileShape_t water = UniformShade_(rShape, 0.0f);
    const auto range = rShading.shades.find(pOccupant->id);
    if (range == rShading.shades.end())
    {
        return TryDrawTileSprite_(rGraphics, LayerSpritePath_(rTile, rLayer, *pOccupant, pMap),
                                  water);
    }
    const DiamondShades_t shades = ResolveWaterShades(rTile, pMap, rShading);
    const ImprovementConfig_t* pArt = pOccupant;
    if (pOccupant->id == rShading.deepLandform || pOccupant->id == rShading.shelfLandform)
    {
        pArt = rTile.FindOccupantConfig(SeaArtLandform(shades, rShading));
        if (!pArt)
        {
            return false;
        }
    }
    ApplyWaterShades(water, shades, rShading.shades.at(pArt->id));
    return TryDrawTileSprite_(rGraphics, LayerSpritePath_(rTile, rLayer, *pArt, pMap), water);
}

// Missing river art: a line from the tile centre to each connected edge's midpoint, or a short
// cross on a river tile with no river neighbor.
void DrawProceduralRiver_(Graphics& rGraphics, const Tile& rTile, const WorldMap* pMap,
                          const TileShape_t& rShape, bool bFogged)
{
    const auto& s = Style().tileRenderer;
    const Color_t color = bFogged ? DimColor_(s.riverColor, s.fogFillDimRatio) : s.riverColor;
    const float width = rShape.east.x - rShape.west.x;
    const float thickness = std::max(1.0f, width * s.riverLineThicknessRatio);
    const float centerX = rShape.center.x;
    const float centerY = rShape.center.y;
    const RiverConnection_t connections =
        pMap ? GetRiverConnections(rTile, *pMap) : RiverConnection_t::None;
    if (connections == RiverConnection_t::None)
    {
        const float stub = width * 0.2f;
        rGraphics.DrawLine(centerX - stub, centerY, centerX + stub, centerY, color, thickness);
        rGraphics.DrawLine(centerX, centerY - stub, centerX, centerY + stub, color, thickness);
        return;
    }
    // North, East, South, West on the grid are the NE, SE, SW, NW diamond edges.
    const struct
    {
        RiverConnection_t direction;
        const TileVertex_t* pFrom;
        const TileVertex_t* pTo;
    } k_Edges[] = {
        {RiverConnection_t::North, &rShape.north, &rShape.east},
        {RiverConnection_t::East, &rShape.east, &rShape.south},
        {RiverConnection_t::South, &rShape.south, &rShape.west},
        {RiverConnection_t::West, &rShape.west, &rShape.north},
    };
    for (const auto& rEdge : k_Edges)
    {
        if (HasRiverConnection(connections, rEdge.direction))
        {
            rGraphics.DrawLine(centerX, centerY, (rEdge.pFrom->x + rEdge.pTo->x) * 0.5f,
                               (rEdge.pFrom->y + rEdge.pTo->y) * 0.5f, color, thickness);
        }
    }
}

bool ShouldSkipLandProceduralOverlays_(const Tile& rTile)
{
    return !rTile.IsLand() || rTile.HasFeature(ImprovementIds::k_Fungus)
           || rTile.HasImprovement(ImprovementIds::k_Forest);
}

void DrawProceduralRockiness_(Graphics& rGraphics, const Tile& rTile, const TileShape_t& rShape,
                              bool bFogged, const Color_t& baseFill)
{
    if (ShouldSkipLandProceduralOverlays_(rTile))
    {
        return;
    }
    const auto& s = Style().tileRenderer;
    const float dim = bFogged ? s.fogFillDimRatio : 1.0f;
    const Rockiness_t rockiness = rTile.GetRockiness();
    if (rockiness != Rockiness_t::Rolling && rockiness != Rockiness_t::Rocky)
    {
        return;
    }
    const Color_t ring =
        DimColor_(rockiness == Rockiness_t::Rocky ? s.rockyRingColor : s.rollingRingColor, dim);
    FillInsetShape_(rGraphics, rShape, s.landformRingOuterInsetRatio, ring);
    FillInsetShape_(rGraphics, rShape, s.landformRingInnerInsetRatio, baseFill);
}

void DrawProceduralMoisture_(Graphics& rGraphics, const Tile& rTile, const TileShape_t& rShape,
                             bool bFogged)
{
    if (ShouldSkipLandProceduralOverlays_(rTile))
    {
        return;
    }
    const auto& s = Style().tileRenderer;
    const float dim = bFogged ? s.fogFillDimRatio : 1.0f;
    const Moisture_t moisture = rTile.GetMoisture();
    if (moisture != Moisture_t::Moist && moisture != Moisture_t::Wet)
    {
        return;
    }
    const Color_t center =
        DimColor_(moisture == Moisture_t::Wet ? s.wetCenterColor : s.moistCenterColor, dim);
    FillInsetShape_(rGraphics, rShape, s.landformRingInnerInsetRatio, center);
}

// The occupant ground art that replaces the moisture base (SMAC's farm ground), if any.
std::string GroundSpritePath_(const Tile& rTile)
{
    const std::string moisture = ToString(rTile.GetMoisture());
    for (const ImprovementConfig_t* pImprovement : rTile.GetImprovements())
    {
        if (!pImprovement)
        {
            continue;
        }
        if (const auto it = pImprovement->groundSprites.find(moisture);
            it != pImprovement->groundSprites.end())
        {
            return PickSpritePath(it->second, rTile.GetX(), rTile.GetY(), pImprovement->id);
        }
    }
    return {};
}

// SMAC's eight directions, NE first and clockwise, on our square grid.
constexpr int k_LinkDirections = 8;
constexpr std::array<std::pair<int, int>, k_LinkDirections> k_LinkDeltas = {{
    {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1},
}};

bool CarriesNetwork_(const Tile& rTile, const OccupantSpriteTiles_t& rNetwork)
{
    return rTile.IsLand()
           && std::ranges::any_of(rNetwork.linkOccupants,
                                  [&rTile](const std::string& rId) { return rTile.HasFeature(rId); });
}

// Road-style networks (layout "links"), as SMAC draws roads and mag tubes: a cell toward every
// land neighbor carrying the same network, the replacing network's cell where both tiles carry
// it, and a hub on a tile that has the network's own occupant but drew no link.
void DrawLinkNetworks_(Graphics& rGraphics, const Tile& rTile, const WorldMap& rMap,
                       const TileShape_t& rShape)
{
    if (!rTile.IsLand())
    {
        return;
    }
    // Networks are found on the tile and its neighbors: a base joins whatever reaches it.
    std::vector<const ImprovementConfig_t*> networks;
    const auto collect = [&networks](const Tile& rAny) {
        for (const ImprovementConfig_t* pImprovement : rAny.GetImprovements())
        {
            if (pImprovement && pImprovement->spriteTiles
                && pImprovement->spriteTiles->layout == SpriteTileLayout_t::Links
                && std::ranges::find(networks, pImprovement) == networks.end())
            {
                networks.push_back(pImprovement);
            }
        }
    };
    collect(rTile);
    std::array<const Tile*, k_LinkDirections> neighbors{};
    for (int dir = 0; dir < k_LinkDirections; ++dir)
    {
        const auto [dx, dy] = k_LinkDeltas[static_cast<std::size_t>(dir)];
        neighbors[static_cast<std::size_t>(dir)] = rMap.GetTile(rTile.GetX() + dx, rTile.GetY() + dy);
        if (neighbors[static_cast<std::size_t>(dir)])
        {
            collect(*neighbors[static_cast<std::size_t>(dir)]);
        }
    }

    std::vector<std::array<bool, k_LinkDirections>> links(networks.size());
    for (std::size_t n = 0; n < networks.size(); ++n)
    {
        const OccupantSpriteTiles_t& rNetwork = *networks[n]->spriteTiles;
        if (!CarriesNetwork_(rTile, rNetwork))
        {
            continue;
        }
        for (std::size_t dir = 0; dir < k_LinkDirections; ++dir)
        {
            links[n][dir] = neighbors[dir] && CarriesNetwork_(*neighbors[dir], rNetwork);
        }
    }
    // A replacing network's link stands in for the one it replaces.
    for (std::size_t n = 0; n < networks.size(); ++n)
    {
        const std::string& rReplaced = networks[n]->spriteTiles->replacesLinksOf;
        for (std::size_t m = 0; m < networks.size(); ++m)
        {
            if (networks[m]->id != rReplaced)
            {
                continue;
            }
            for (std::size_t dir = 0; dir < k_LinkDirections; ++dir)
            {
                links[m][dir] = links[m][dir] && !links[n][dir];
            }
        }
    }

    bool bAnyLink = false;
    for (std::size_t n = 0; n < networks.size(); ++n)
    {
        const std::string& rPattern = networks[n]->spriteTiles->land;
        for (std::size_t dir = 0; dir < k_LinkDirections; ++dir)
        {
            if (links[n][dir])
            {
                bAnyLink = true;
                const unsigned cell = 1 + ((static_cast<unsigned>(dir) + 2) & 7u);
                std::string path = rPattern;
                path.replace(path.find(k_MaskToken), k_MaskToken.size(), std::to_string(cell));
                (void)TryDrawTileSprite_(rGraphics, path, rShape);
            }
        }
    }
    for (std::size_t n = 0; n < networks.size(); ++n)
    {
        const ImprovementConfig_t& rConfig = *networks[n];
        const OccupantSpriteTiles_t& rNetwork = *rConfig.spriteTiles;
        // Only the network's own occupant draws a hub (a base never does), and a plain network
        // only when no link of any network was drawn.
        const bool bOwnOnly = rTile.HasImprovement(rConfig.id)
                              && std::ranges::none_of(rNetwork.linkOccupants, [&](const std::string& rId) {
                                     return rId != rConfig.id && rTile.HasFeature(rId);
                                 });
        const bool bNoOwnLink = std::ranges::none_of(links[n], [](bool bLink) { return bLink; });
        if (bOwnOnly && bNoOwnLink && (!rNetwork.replacesLinksOf.empty() || !bAnyLink))
        {
            std::string path = rNetwork.land;
            path.replace(path.find(k_MaskToken), k_MaskToken.size(), "0");
            (void)TryDrawTileSprite_(rGraphics, path, rShape);
        }
    }
}

int YieldOf_(const TileResources_t& rYield, YieldStat_t stat)
{
    switch (stat)
    {
        case YieldStat_t::Nutrients:
            return rYield.nutrients;
        case YieldStat_t::Minerals:
            return rYield.minerals;
        case YieldStat_t::Energy:
            return rYield.energy;
    }
    throw std::runtime_error("YieldOf_: unhandled YieldStat_t");
}

// An improvement's object sprite on this tile's surface: a yield row when it has them, else a
// variant of its sprite paths. Empty when it has none.
std::string ObjectSpritePath_(const ImprovementConfig_t& rConfig, const Tile& rTile,
                              const TileRenderer::YieldLookup_t& rYieldOf)
{
    if (rConfig.spriteYieldRows)
    {
        const std::vector<std::string>& rRows =
            rTile.IsWater() ? rConfig.spriteYieldRows->paths.sea : rConfig.spriteYieldRows->paths.land;
        if (rRows.empty())
        {
            return {};
        }
        const int yield = rYieldOf ? YieldOf_(rYieldOf(rTile), rConfig.spriteYieldRows->stat) : 1;
        const int row = std::clamp(yield - 1, 0, static_cast<int>(rRows.size()) - 1);
        return rRows[static_cast<std::size_t>(row)];
    }
    return VariantSpritePath_(rConfig, rTile);
}

// Configured object art that failed to load: a 2 × 2 magenta and black checker at the seat, so
// the gap is obvious rather than silent.
void DrawMissingArt_(Graphics& rGraphics, const TileShape_t& rShape)
{
    const auto& s = Style().tileRenderer;
    const float size = (rShape.east.x - rShape.west.x) * s.missingArtSizeRatio;
    const float half = size * 0.5f;
    const float seatX = (rShape.west.x + rShape.north.x + rShape.east.x + rShape.south.x) * 0.25f;
    const float seatY = (rShape.west.y + rShape.north.y + rShape.east.y + rShape.south.y) * 0.25f;
    for (int cell = 0; cell < 4; ++cell)
    {
        const float x = seatX - half + static_cast<float>(cell % 2) * half;
        const float y = seatY - half + static_cast<float>(cell / 2) * half;
        rGraphics.DrawFilledRect(x, y, half, half,
                                 (cell == 0 || cell == 3) ? s.missingArtColor : s.missingArtAltColor);
    }
}

// An object sprite, or the missing-art checker when its configured path does not load.
void DrawObject_(Graphics& rGraphics, const ImprovementConfig_t& rConfig, const std::string& path,
                 const TileShape_t& rShape)
{
    if (!TryDrawOccupantPath_(rGraphics, rConfig, path, rShape, Color_t::White()))
    {
        DrawMissingArt_(rGraphics, rShape);
    }
}

} // namespace

size_t PickSpriteIndex(int tileX, int tileY, std::string_view contentId, size_t count)
{
    if (count == 0)
    {
        return 0;
    }
    std::uint64_t hash = k_FnvOffset;
    MixHash_(hash, static_cast<std::uint64_t>(static_cast<std::uint32_t>(tileX)));
    MixHash_(hash, static_cast<std::uint64_t>(static_cast<std::uint32_t>(tileY)));
    for (const unsigned char ch : contentId)
    {
        MixHash_(hash, ch);
    }
    return static_cast<size_t>(hash % count);
}

const std::string& PickSpritePath(const std::vector<std::string>& paths, int tileX, int tileY,
                                  std::string_view contentId)
{
    static const std::string k_Empty;
    if (paths.empty())
    {
        return k_Empty;
    }
    return paths[PickSpriteIndex(tileX, tileY, contentId, paths.size())];
}

Color_t TileRenderer::FillColor(const Tile& rTile, bool bFogged)
{
    const auto& s = Style().tileRenderer;
    const int elevation = rTile.GetElevation();
    Color_t fill{};

    // Fungus is a terrain overlay sprite, not a solid fill — keep elevation/forest under it.
    if (rTile.HasImprovement(ImprovementIds::k_Forest)
        && !rTile.HasFeature(ImprovementIds::k_Fungus))
    {
        fill = s.forestColor;
    }
    else if (rTile.IsWater())
    {
        const ElevationRulesConfig_t& rRules = rTile.MapRules();
        // Water: darker at the floor, lighter one meter below ocean level.
        const float t = Remap01_(static_cast<float>(elevation),
                                 static_cast<float>(rRules.minElevationMeters),
                                 static_cast<float>(rRules.oceanLevelMeters - 1));
        fill = LerpColor_(s.waterLowColor, s.waterHighColor, t);
    }
    else
    {
        const ElevationRulesConfig_t& rRules = rTile.MapRules();
        // Land: darker at ocean level, lighter at the map's maximum elevation.
        const float t = Remap01_(static_cast<float>(elevation),
                                 static_cast<float>(rRules.oceanLevelMeters),
                                 static_cast<float>(rRules.maxElevationMeters));
        fill = LerpColor_(s.landLowColor, s.landHighColor, t);
    }

    if (bFogged)
    {
        fill = DimColor_(fill, s.fogFillDimRatio);
    }
    return fill;
}

TileShape_t TileRenderer::FlatTileShape(float x, float y, float size)
{
    const float width = size;
    const float height = size * k_IsoHeightRatio;
    const auto vertex = [&](float u, float v) {
        return TileVertex_t{x + width * u, y + height * v};
    };
    return TileShape_t{vertex(0.5f, 0.5f), vertex(0.0f, 0.5f), vertex(0.5f, 0.0f),
                       vertex(1.0f, 0.5f), vertex(0.5f, 1.0f)};
}

void TileRenderer::Render(Graphics& rGraphics, const Tile& rTile, const TileShape_t& rShape,
                          bool bFogged, const WorldMap* pMap, const YieldLookup_t& rYieldOf)
{
    RenderTerrain(rGraphics, rTile, rShape, bFogged, pMap);
    RenderObjects(rGraphics, rTile, rShape, rYieldOf);
}

void TileRenderer::RenderTerrain(Graphics& rGraphics, const Tile& rTile, const TileShape_t& rShape,
                                 bool bFogged, const WorldMap* pMap)
{
    const auto& s = Style().tileRenderer;
    const Color_t baseFill = FillColor(rTile, bFogged);
    const TileShape_t terrain = TerrainShape_(rShape, rTile, bFogged);

    rGraphics.FillTileShape(rShape, baseFill);

    // The coast covers the terrain layers and sits under rivers, roads and improvements.
    bool bCoastDrawn = false;
    auto drawCoast = [&]() {
        if (bCoastDrawn)
        {
            return;
        }
        bCoastDrawn = true;
        if (pMap)
        {
            DrawCoastOverlay_(rGraphics, rTile, *pMap, rShape, bFogged ? s.fogLandShade : 0.0f);
        }
    };

    for (const TileLayer_t& rLayer : ResolveTileLayers(rTile))
    {
        if (rLayer.type > TileLayerType_t::Vegetation)
        {
            drawCoast();
        }
        if (rLayer.type == TileLayerType_t::Road)
        {
            if (pMap)
            {
                DrawLinkNetworks_(rGraphics, rTile, *pMap, terrain);
            }
            continue;
        }
        if (rLayer.type == TileLayerType_t::Improvement || !rLayer.contentId.has_value())
        {
            continue;
        }
        if (rLayer.type == TileLayerType_t::Moisture)
        {
            const std::string ground = GroundSpritePath_(rTile);
            if (!ground.empty() && TryDrawTileSprite_(rGraphics, ground, terrain))
            {
                continue;
            }
        }
        if (rLayer.type == TileLayerType_t::Landform && rTile.IsWater())
        {
            (void)TryDrawWaterLandform_(rGraphics, rTile, rLayer, pMap, rShape);
            continue;
        }
        const bool bFungusLayer = rLayer.type == TileLayerType_t::Vegetation
                                  && *rLayer.contentId == TileLayerContent::k_Fungus;
        const TileShape_t layerShape = rLayer.type == TileLayerType_t::Rockiness
                                           ? RockinessShape_(terrain, rTile, pMap)
                                           : terrain;

        if (TryDrawLayerSprite_(rGraphics, rTile, rLayer, pMap, layerShape))
        {
            continue;
        }
        // Per-layer procedural cues when that layer's sprite is missing.
        if (rLayer.type == TileLayerType_t::Moisture)
        {
            DrawProceduralMoisture_(rGraphics, rTile, rShape, bFogged);
        }
        else if (bFungusLayer)
        {
            rGraphics.FillTileShape(layerShape, s.fungusColor);
        }
        else if (rLayer.type == TileLayerType_t::Rockiness)
        {
            DrawProceduralRockiness_(rGraphics, rTile, layerShape, bFogged, baseFill);
        }
        else if (rLayer.type == TileLayerType_t::River)
        {
            DrawProceduralRiver_(rGraphics, rTile, pMap, rShape, bFogged);
        }
    }
    drawCoast();
    // Fog hazes the terrain layers; objects draw clear on top of it, as in SMAC.
    if (bFogged)
    {
        rGraphics.FillTileShape(rShape, s.fogHazeColor);
    }
}

void TileRenderer::RenderObjects(Graphics& rGraphics, const Tile& rTile, const TileShape_t& rShape,
                                 const YieldLookup_t& rYieldOf)
{
    // Optional terrain bonuses and the Monolith sit in GetTerrainFeatures, not improvements; SMAC
    // draws the bonuses before the improvements.
    for (const ImprovementConfig_t* pFeature : rTile.GetTerrainFeatures())
    {
        if (!pFeature || SurfaceSpritePaths_(*pFeature, rTile).empty())
        {
            continue;
        }
        // Axes (Flat/Moist/…), water bands and landmarks are drawn via layers; skip duplicates.
        if (pFeature->id == "Flat" || pFeature->id == "Rolling" || pFeature->id == "Rocky"
            || pFeature->id == "Arid" || pFeature->id == "Moist" || pFeature->id == "Wet"
            || pFeature->id == "Water" || pFeature->id == "Ocean" || pFeature->id == "OceanShelf"
            || pFeature->id == "Fungus" || pFeature->id == "River"
            || std::ranges::find(pFeature->tags, "landmark") != pFeature->tags.end())
        {
            continue;
        }
        DrawObject_(rGraphics, *pFeature, VariantSpritePath_(*pFeature, rTile), rShape);
    }

    std::vector<std::string> hidden;
    for (const ImprovementConfig_t* pImprovement : rTile.GetImprovements())
    {
        if (pImprovement)
        {
            hidden.insert(hidden.end(), pImprovement->hidesSpritesOf.begin(),
                          pImprovement->hidesSpritesOf.end());
        }
    }
    for (const ImprovementConfig_t* pImprovement : rTile.GetImprovements())
    {
        if (!pImprovement || std::ranges::find(hidden, pImprovement->id) != hidden.end())
        {
            continue;
        }
        const std::string path = ObjectSpritePath_(*pImprovement, rTile, rYieldOf);
        if (!path.empty())
        {
            DrawObject_(rGraphics, *pImprovement, path, rShape);
        }
    }
}

} // namespace ac
