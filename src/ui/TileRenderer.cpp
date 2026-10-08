#include "ui/TileRenderer.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementIds.h"
#include "game/map/Tile.h"
#include "game/map/TileLayer.h"
#include "game/map/TileLayerResolver.h"
#include "game/map/MapUtils.h"
#include "game/map/WorldMap.h"
#include "graphics/Graphics.h"
#include "ui/CoastOverlay.h"
#include "ui/SpriteLibrary.h"
#include "ui/TileAutotile.h"
#include "ui/TileShapeGeometry.h"
#include "ui/WaterShading.h"
#include "ui/style/UiStyle.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ac
{

namespace
{

constexpr std::uint64_t k_FnvOffset = 14695981039346656037ull;
constexpr std::uint64_t k_FnvPrime  = 1099511628211ull;

void MixHash_(std::uint64_t& rHash, std::uint64_t value)
{
    rHash ^= value;
    rHash *= k_FnvPrime;
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

constexpr std::size_t k_WaterPart = 0;
constexpr std::size_t k_ShorePart = 1;
constexpr std::array<std::string_view, 2> k_CoastPartNames = {"water", "shore"};
constexpr std::array<char, k_DiamondCornerCount> k_CoastCornerNames = {'w', 'n', 'e', 's'};
constexpr std::array<std::string_view, 8> k_CoastCaseNames = {"1", "2", "3", "4",
                                                              "5", "6", "7", "7_alt"};
constexpr std::size_t k_CoastAlternateCase = 7;

const std::vector<std::string>& SurfaceSpritePaths_(const ImprovementConfig_t& rOccupant,
                                                    const Tile& rTile)
{
    return rTile.IsWater() ? rOccupant.spritePaths.sea : rOccupant.spritePaths.land;
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

constexpr int k_LinkDirections = static_cast<int>(k_RingNeighbors.size());

bool CarriesNetwork_(const Tile& rTile, const OccupantSpriteTiles_t& rNetwork)
{
    return rTile.IsLand()
           && std::ranges::any_of(rNetwork.linkOccupants,
                                  [&rTile](const std::string& rId) { return rTile.HasFeature(rId); });
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

TileRenderer::TileRenderer(SpriteLibrary& rSprites, const TileRendererStyle_t& rStyle)
    : m_rSprites(rSprites)
    , m_rStyle(rStyle)
    , m_coastPaths(BuildCoastPaths_(rStyle.coastSpriteDir))
{
}

TileRenderer::CoastPaths_t TileRenderer::BuildCoastPaths_(const std::string& rDirectory)
{
    CoastPaths_t paths;
    for (std::size_t part = 0; part < k_CoastPartCount; ++part)
    {
        for (std::size_t corner = 0; corner < k_DiamondCornerCount; ++corner)
        {
            for (std::size_t coastCase = 0; coastCase < k_CoastCaseCount; ++coastCase)
            {
                std::string& rPath = paths[part][corner][coastCase];
                rPath = rDirectory;
                rPath += '/';
                rPath += k_CoastPartNames[part];
                rPath += '_';
                rPath += k_CoastCornerNames[corner];
                rPath += '_';
                rPath += k_CoastCaseNames[coastCase];
                rPath += ".png";
            }
        }
    }
    return paths;
}

bool TileRenderer::TryDrawSprite_(Graphics& rGraphics, const std::string& path, float x, float y,
                                  float width, float height, const Color_t& tint) const
{
    return m_rSprites.Ensure(path) && rGraphics.DrawSprite(path, x, y, width, height, tint);
}

// Tile art is palette indices, so it draws only when the palette loads too.
bool TileRenderer::TryDrawTileSprite_(Graphics& rGraphics, const std::string& path,
                                      const TileShape_t& rShape) const
{
    const std::string& palette = m_rStyle.palettePath;
    return m_rSprites.Ensure(path) && m_rSprites.Ensure(palette)
           && rGraphics.DrawTileSprite(path, palette, rShape);
}

// Land in sight keeps the relief's shades; out of sight SMAC shades land a flat
// fog_land_shade instead of lighting its slopes. Terrain layers on water draw as painted.
TileShape_t TileRenderer::TerrainShape_(const TileShape_t& rShape, const Tile& rTile,
                                        bool bFogged) const
{
    if (!rTile.IsLand())
    {
        return UniformShade_(rShape, 0.0f);
    }
    return bFogged ? UniformShade_(rShape, m_rStyle.fogLandShade) : rShape;
}

const std::string& TileRenderer::CoastSpritePath_(std::size_t part,
                                                  const CoastCornerArt_t& rArt) const
{
    const std::size_t coastCase = rArt.bAlternate ? k_CoastAlternateCase : rArt.waterMask - 1u;
    return m_coastPaths[part][static_cast<std::size_t>(rArt.corner)][coastCase];
}

// Ocean and shore art baked per diamond corner (extract_terrain.py). The water is shaded by the
// depths around the land tile's own vertices, so it meets the neighboring water's shading at
// their shared corners; the shore draws at shoreShade.
void TileRenderer::DrawCoastOverlay_(Graphics& rGraphics, const Tile& rTile, const WorldMap& rMap,
                                     const TileShape_t& rShape, float shoreShade) const
{
    const CoastOverlay_t overlay = ResolveCoastOverlay(rTile, rMap);
    if (std::ranges::none_of(overlay.corners,
                             [](const CoastCornerArt_t& rArt) { return rArt.waterMask != 0; }))
    {
        return;
    }
    const WaterShadingStyle_t& rShading = m_rStyle.waterShading;
    TileShape_t water = UniformShade_(rShape, 0.0f);
    ApplyWaterShades(water, ResolveWaterShades(rTile, &rMap, rShading),
                     rShading.shades.at(rShading.coastShades));
    for (const CoastCornerArt_t& rArt : overlay.corners)
    {
        if (rArt.waterMask != 0)
        {
            (void)TryDrawTileSprite_(rGraphics, CoastSpritePath_(k_WaterPart, rArt), water);
        }
    }
    const TileShape_t shore = UniformShade_(rShape, shoreShade);
    for (const CoastCornerArt_t& rArt : overlay.corners)
    {
        if (rArt.waterMask != 0)
        {
            (void)TryDrawTileSprite_(rGraphics, CoastSpritePath_(k_ShorePart, rArt), shore);
        }
    }
}

// Object sprites are a ter1.pcx cell: a tile-high footprint plus their overhang ratio. SMAC draws
// the cell from the tile's top corner down, seated at the mean of the tile's four corners rather
// than its raised centre; the art sits high in its cells, so a tile bonus lands mid-tile.
bool TileRenderer::TryDrawOccupantPath_(Graphics& rGraphics, const ImprovementConfig_t& rOccupant,
                                        const std::string& path, const TileShape_t& rShape,
                                        const Color_t& tint) const
{
    const float width = rShape.east.x - rShape.west.x;
    const float height = width * k_IsoHeightRatio;
    const float overhang = height * rOccupant.spriteOverhangRatio;
    const auto [seatX, seatY] = SeatOf(rShape);
    return TryDrawSprite_(rGraphics, path, seatX - width * 0.5f, seatY - height * 0.5f, width,
                          height + overhang, tint);
}

// A terrain layer: a baked terrain diamond drawn on the shape.
bool TileRenderer::TryDrawLayerSprite_(Graphics& rGraphics, const Tile& rTile,
                                       const TileLayer_t& rLayer, const WorldMap* pMap,
                                       const TileShape_t& rShape) const
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
bool TileRenderer::TryDrawWaterLandform_(Graphics& rGraphics, const Tile& rTile,
                                         const TileLayer_t& rLayer, const WorldMap* pMap,
                                         const TileShape_t& rShape) const
{
    const ImprovementConfig_t* pOccupant = FindLayerOccupant_(rTile, rLayer);
    if (!pOccupant)
    {
        return false;
    }
    const WaterShadingStyle_t& rShading = m_rStyle.waterShading;
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

// Road-style networks (layout "links"), as SMAC draws roads and mag tubes: a cell toward every
// land neighbor carrying the same network, the replacing network's cell where both tiles carry
// it, and a hub on a tile that has the network's own occupant but drew no link.
void TileRenderer::DrawLinkNetworks_(Graphics& rGraphics, const Tile& rTile, const WorldMap& rMap,
                                     const TileShape_t& rShape) const
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
        const LatticeOffset_t& offset = k_RingNeighbors[static_cast<std::size_t>(dir)];
        neighbors[static_cast<std::size_t>(dir)] =
            GetTileAtLatticeOffset(rMap, rTile, offset.p, offset.q);
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

// Configured object art that failed to load: a 2 × 2 magenta and black checker at the seat, so
// the gap is obvious rather than silent.
void TileRenderer::DrawMissingArt_(Graphics& rGraphics, const TileShape_t& rShape) const
{
    const float size = (rShape.east.x - rShape.west.x) * m_rStyle.missingArtSizeRatio;
    const float half = size * 0.5f;
    const auto [seatX, seatY] = SeatOf(rShape);
    for (int cell = 0; cell < 4; ++cell)
    {
        const float x = seatX - half + static_cast<float>(cell % 2) * half;
        const float y = seatY - half + static_cast<float>(cell / 2) * half;
        rGraphics.DrawFilledRect(x, y, half, half,
                                 (cell == 0 || cell == 3) ? m_rStyle.missingArtColor
                                                          : m_rStyle.missingArtAltColor);
    }
}

// An object sprite, or the missing-art checker when its configured path does not load.
void TileRenderer::DrawObject_(Graphics& rGraphics, const ImprovementConfig_t& rConfig,
                               const std::string& path, const TileShape_t& rShape) const
{
    if (!TryDrawOccupantPath_(rGraphics, rConfig, path, rShape, Color_t::White()))
    {
        DrawMissingArt_(rGraphics, rShape);
    }
}

Color_t TileRenderer::FillColor(const Tile& rTile, bool bFogged) const
{
    const TileRendererStyle_t& s = m_rStyle;
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

void TileRenderer::Render(Graphics& rGraphics, const Tile& rTile, const TileShape_t& rShape,
                          bool bFogged, const WorldMap* pMap, const YieldLookup_t& rYieldOf) const
{
    RenderTerrain(rGraphics, rTile, rShape, bFogged, pMap);
    RenderObjects(rGraphics, rTile, rShape, rYieldOf);
}

void TileRenderer::RenderTerrain(Graphics& rGraphics, const Tile& rTile, const TileShape_t& rShape,
                                 bool bFogged, const WorldMap* pMap) const
{
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
            DrawCoastOverlay_(rGraphics, rTile, *pMap, rShape,
                              bFogged ? m_rStyle.fogLandShade : 0.0f);
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
        // Missing terrain art draws nothing; the fill underneath remains.
        (void)TryDrawLayerSprite_(rGraphics, rTile, rLayer, pMap, terrain);
    }
    drawCoast();
    // Fog hazes the terrain layers; objects draw clear on top of it, as in SMAC.
    if (bFogged)
    {
        rGraphics.FillTileShape(rShape, m_rStyle.fogHazeColor);
    }
}

void TileRenderer::RenderObjects(Graphics& rGraphics, const Tile& rTile, const TileShape_t& rShape,
                                 const YieldLookup_t& rYieldOf) const
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
