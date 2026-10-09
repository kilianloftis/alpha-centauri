#include "ui/TileRenderer.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"
#include "game/map/MapUtils.h"
#include "game/map/WorldMap.h"
#include "graphics/Graphics.h"
#include "ui/CoastOverlay.h"
#include "ui/SpriteLibrary.h"
#include "ui/TileAutotile.h"
#include "ui/TileShapeGeometry.h"
#include "ui/WaterShading.h"
#include "ui/style/UiStyle.h"
#include "ui/world/MapAppearance.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
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

const OccupantArt_t& ArtOf_(const ImprovementConfig_t& rOccupant)
{
    if (!rOccupant.art)
    {
        throw std::logic_error("TileRenderer: occupant '" + rOccupant.id + "' has no art");
    }
    return *rOccupant.art;
}

const std::string& VariantSpritePath_(const ImprovementConfig_t& rOccupant, const Tile& rTile)
{
    const OccupantSpritePaths_t& rVariants =
        std::get<OccupantSpritePaths_t>(ArtOf_(rOccupant).sprites);
    return PickSpritePath(rVariants.ForSurface(rTile.IsWater()), rTile.GetX(), rTile.GetY(),
                          rOccupant.id);
}

using NeighborRule_t = std::function<bool(const Tile& rNeighbor)>;

bool HasOccupantId_(const MapAppearance& rAppearance, const Tile& rTile, std::string_view id)
{
    return rAppearance.OccupantsOf(rTile).ForEach(
        [id](const ImprovementConfig_t& rOccupant) { return rOccupant.id == id; });
}

// Moisture tiles connect to water and to land at least as wet, fading out toward drier land;
// every other tile set connects to neighbors with the same occupant.
NeighborRule_t LayerNeighborRule_(const MapAppearance& rAppearance, const Tile& rTile,
                                  const ImprovementConfig_t& rOccupant)
{
    if (ArtOf_(rOccupant).layer == ArtLayer_t::Moisture)
    {
        return [&rTile](const Tile& rNeighbor) {
            return rNeighbor.IsWater()
                   || static_cast<int>(rNeighbor.GetMoisture())
                          >= static_cast<int>(rTile.GetMoisture());
        };
    }
    return [&rAppearance, &rOccupant](const Tile& rNeighbor) {
        return HasOccupantId_(rAppearance, rNeighbor, rOccupant.id);
    };
}

const std::string& TileSpritePath_(const OccupantTileSet_t& rTiles, const Tile& rTile,
                                   const WorldMap& rMap, const NeighborRule_t& matches)
{
    static const std::string k_Empty;
    const std::vector<std::string>& rPaths = rTiles.paths.ForSurface(rTile.IsWater());
    if (rPaths.empty())
    {
        return k_Empty;
    }
    const std::uint8_t mask = ResolveTileMask(rTiles.layout, rTile, rMap, matches);
    return rPaths.at(mask);
}

const std::string& LayerSpritePath_(const Tile& rTile, const ImprovementConfig_t& rOccupant,
                                    const MapAppearance& rAppearance)
{
    if (const auto* pTiles = std::get_if<OccupantTileSet_t>(&ArtOf_(rOccupant).sprites))
    {
        return TileSpritePath_(*pTiles, rTile, rAppearance.Map(),
                               LayerNeighborRule_(rAppearance, rTile, rOccupant));
    }
    return VariantSpritePath_(rOccupant, rTile);
}

// The occupant ground art that replaces the moisture base (SMAC's farm ground), if any.
std::string GroundSpritePath_(const MapAppearance& rAppearance, const Tile& rTile)
{
    const std::string moisture = ToString(rTile.GetMoisture());
    std::string path;
    rAppearance.OccupantsOf(rTile).ForEach([&](const ImprovementConfig_t& rOccupant) {
        if (!rOccupant.art)
        {
            return false;
        }
        const auto it = rOccupant.art->ground.find(moisture);
        if (it == rOccupant.art->ground.end())
        {
            return false;
        }
        path = PickSpritePath(it->second, rTile.GetX(), rTile.GetY(), rOccupant.id);
        return true;
    });
    return path;
}

constexpr int k_LinkDirections = static_cast<int>(k_RingNeighbors.size());

const OccupantTileSet_t& NetworkOf_(const ImprovementConfig_t& rConfig)
{
    return std::get<OccupantTileSet_t>(ArtOf_(rConfig).sprites);
}

bool CarriesNetwork_(const MapAppearance& rAppearance, const Tile& rTile,
                     const OccupantTileSet_t& rNetwork)
{
    return rTile.IsLand()
           && std::ranges::any_of(rNetwork.linkOccupants, [&](const std::string& rId) {
                  return HasOccupantId_(rAppearance, rTile, rId);
              });
}

bool HasLayerArt_(const ImprovementConfig_t& rOccupant, ArtLayer_t layer)
{
    return rOccupant.art && rOccupant.art->layer == layer;
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

// An object's sprite on this tile's surface: a yield row when it has them, else a variant of
// its sprite paths. Empty when it has none.
// TODO: confirm which yield SMAC uses for remembered farm structures
std::string ObjectSpritePath_(const ImprovementConfig_t& rConfig, const Tile& rTile,
                              const TileRenderer::YieldLookup_t& rYieldOf)
{
    if (const auto* pRows = std::get_if<OccupantYieldRows_t>(&ArtOf_(rConfig).sprites))
    {
        const std::vector<std::string>& rRows = pRows->paths.ForSurface(rTile.IsWater());
        if (rRows.empty())
        {
            return {};
        }
        const int yield = rYieldOf ? YieldOf_(rYieldOf(rTile), pRows->stat) : 1;
        const int row = std::clamp(yield - 1, 0, static_cast<int>(rRows.size()) - 1);
        return rRows[static_cast<std::size_t>(row)];
    }
    return VariantSpritePath_(rConfig, rTile);
}

const WaterShadeRange_t& DepthShadeOf_(const ImprovementConfig_t& rLandform)
{
    if (!rLandform.art || !rLandform.art->depthShade)
    {
        throw std::runtime_error("TileRenderer: landform '" + rLandform.id
                                 + "' has no depth_shade");
    }
    return *rLandform.art->depthShade;
}

const WaterShadeRange_t& DepthShadeOf_(const Tile& rTile, const std::string& rLandformId)
{
    const ImprovementConfig_t* pLandform = rTile.FindOccupantConfig(rLandformId);
    if (!pLandform)
    {
        throw std::runtime_error("TileRenderer: landform '" + rLandformId
                                 + "' is not in the occupant registry");
    }
    return DepthShadeOf_(*pLandform);
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

// Land in sight keeps the relief's shades; under fog SMAC shades land a flat fog_land_shade
// instead of lighting its slopes. Terrain layers on water draw as painted.
TileShape_t TileRenderer::TerrainShape_(const TileShape_t& rShape, const Tile& rTile,
                                        TileCover_t cover) const
{
    if (!rTile.IsLand())
    {
        return UniformShade_(rShape, 0.0f);
    }
    return cover == TileCover_t::Fog ? UniformShade_(rShape, m_rStyle.fogLandShade) : rShape;
}

// SMAC draws keyed overlays (fungus, forest, jungle, landmark cells) with shade 0; only the
// moisture base, rockiness and river take slope light. Fog still darkens them flatly.
TileShape_t TileRenderer::OverlayShape_(const TileShape_t& rShape, const Tile& rTile,
                                        TileCover_t cover) const
{
    if (!rTile.IsLand())
    {
        return UniformShade_(rShape, 0.0f);
    }
    return cover == TileCover_t::Fog ? UniformShade_(rShape, m_rStyle.fogLandShade)
                                    : UniformShade_(rShape, 0.0f);
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
    ApplyWaterShades(water, ResolveWaterShades(rTile, rMap, rShading),
                     DepthShadeOf_(rTile, rShading.coastShades));
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
bool TileRenderer::TryDrawOccupantPath_(Graphics& rGraphics, const OccupantArt_t& rArt,
                                        const std::string& path, const TileShape_t& rShape,
                                        const Color_t& tint) const
{
    const float width = rShape.east.x - rShape.west.x;
    const float height = width * k_IsoHeightRatio;
    const float overhang = height * rArt.overhang;
    const auto [originX, originY] = FootprintOrigin(rShape);
    return TryDrawSprite_(rGraphics, path, originX, originY, width, height + overhang, tint);
}

// A terrain occupant's baked diamond drawn on the shape.
bool TileRenderer::TryDrawLayerSprite_(Graphics& rGraphics, const Tile& rTile,
                                       const ImprovementConfig_t& rOccupant,
                                       const MapAppearance& rAppearance,
                                       const TileShape_t& rShape) const
{
    return TryDrawTileSprite_(rGraphics, LayerSpritePath_(rTile, rOccupant, rAppearance), rShape);
}

// Water art shaded per vertex by depth within the shade range of the landform's depth_shade; a
// landform without one draws as painted. The deep and shelf landforms trade art by depth, as SMAC
// picks its deep or shelf texture per tile.
bool TileRenderer::TryDrawWaterLandform_(Graphics& rGraphics, const Tile& rTile,
                                         const ImprovementConfig_t& rOccupant,
                                         const MapAppearance& rAppearance,
                                         const TileShape_t& rShape) const
{
    TileShape_t water = UniformShade_(rShape, 0.0f);
    if (!rOccupant.art->depthShade)
    {
        return TryDrawTileSprite_(rGraphics, LayerSpritePath_(rTile, rOccupant, rAppearance),
                                  water);
    }
    const WaterShadingStyle_t& rShading = m_rStyle.waterShading;
    const DiamondShades_t shades = ResolveWaterShades(rTile, rAppearance.Map(), rShading);
    const ImprovementConfig_t* pArt = &rOccupant;
    if (rOccupant.id == rShading.deepLandform || rOccupant.id == rShading.shelfLandform)
    {
        pArt = rTile.FindOccupantConfig(SeaArtLandform(shades, rShading));
        if (!pArt || !pArt->art)
        {
            return false;
        }
    }
    ApplyWaterShades(water, shades, DepthShadeOf_(*pArt));
    return TryDrawTileSprite_(rGraphics, LayerSpritePath_(rTile, *pArt, rAppearance), water);
}

// Road-style networks (layout "links"), as SMAC draws roads and mag tubes: a cell toward every
// land neighbor carrying the same network, the replacing network's cell where both tiles carry
// it, and a hub on a tile that has the network's own occupant but drew no link.
void TileRenderer::DrawLinkNetworks_(Graphics& rGraphics, const Tile& rTile,
                                     const MapAppearance& rAppearance,
                                     const TileShape_t& rShape) const
{
    const WorldMap& rMap = rAppearance.Map();
    if (!rTile.IsLand())
    {
        return;
    }
    // Networks are found on the tile and its neighbors: a base joins whatever reaches it.
    std::vector<const ImprovementConfig_t*> networks;
    const auto collect = [&networks, &rAppearance](const Tile& rAny) {
        for (const ImprovementConfig_t* pImprovement : rAppearance.OccupantsOf(rAny).improvements)
        {
            if (pImprovement && HasLayerArt_(*pImprovement, ArtLayer_t::Road)
                && !NetworkOf_(*pImprovement).paths.land.empty()
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
        const OccupantTileSet_t& rNetwork = NetworkOf_(*networks[n]);
        if (!CarriesNetwork_(rAppearance, rTile, rNetwork))
        {
            continue;
        }
        for (std::size_t dir = 0; dir < k_LinkDirections; ++dir)
        {
            links[n][dir] = neighbors[dir] && CarriesNetwork_(rAppearance, *neighbors[dir], rNetwork);
        }
    }
    // A replacing network's link stands in for the one it replaces.
    for (std::size_t n = 0; n < networks.size(); ++n)
    {
        const std::string& rReplaced = NetworkOf_(*networks[n]).replacesLinksOf;
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
        const std::vector<std::string>& rCells = NetworkOf_(*networks[n]).paths.land;
        for (std::size_t dir = 0; dir < k_LinkDirections; ++dir)
        {
            if (links[n][dir])
            {
                bAnyLink = true;
                const unsigned cell = 1 + ((static_cast<unsigned>(dir) + 2) & 7u);
                (void)TryDrawTileSprite_(rGraphics, rCells.at(cell), rShape);
            }
        }
    }
    for (std::size_t n = 0; n < networks.size(); ++n)
    {
        const ImprovementConfig_t& rConfig = *networks[n];
        const OccupantTileSet_t& rNetwork = NetworkOf_(rConfig);
        // Only the network's own occupant draws a hub (a base never does), and a plain network
        // only when no link of any network was drawn.
        const TileOccupants_t occupants = rAppearance.OccupantsOf(rTile);
        const bool bOwnOnly = std::ranges::find(occupants.improvements, &rConfig)
                                  != occupants.improvements.end()
                              && std::ranges::none_of(rNetwork.linkOccupants, [&](const std::string& rId) {
                                     return rId != rConfig.id
                                            && HasOccupantId_(rAppearance, rTile, rId);
                                 });
        const bool bNoOwnLink = std::ranges::none_of(links[n], [](bool bLink) { return bLink; });
        if (bOwnOnly && bNoOwnLink && (!rNetwork.replacesLinksOf.empty() || !bAnyLink))
        {
            (void)TryDrawTileSprite_(rGraphics, rNetwork.paths.land.at(0), rShape);
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
    if (!TryDrawOccupantPath_(rGraphics, ArtOf_(rConfig), path, rShape, Color_t::White()))
    {
        DrawMissingArt_(rGraphics, rShape);
    }
}

Color_t TileRenderer::FillColor(const Tile& rTile, const MapAppearance& rAppearance) const
{
    const TileRendererStyle_t& s = m_rStyle;
    const TileCover_t cover = rAppearance.CoverOf(rTile);
    if (cover == TileCover_t::Shroud)
    {
        return s.shroudColor;
    }
    const int elevation = rTile.GetElevation();
    Color_t fill{};

    std::optional<Color_t> occupantFill;
    rAppearance.OccupantsOf(rTile).ForEach([&occupantFill](const ImprovementConfig_t& rOccupant) {
        if (rOccupant.art && rOccupant.art->fillColor)
        {
            occupantFill = rOccupant.art->fillColor;
        }
        return false;
    });

    if (occupantFill)
    {
        fill = *occupantFill;
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

    if (cover == TileCover_t::Fog)
    {
        fill = DimColor_(fill, s.fogFillDimRatio);
    }
    return fill;
}

void TileRenderer::DrawTerrainLayer_(Graphics& rGraphics, const Tile& rTile, ArtLayer_t layer,
                                     const MapAppearance& rAppearance, const TileShape_t& rShape,
                                     const TileShape_t& rTerrain) const
{
    if (layer == ArtLayer_t::Moisture)
    {
        const std::string ground = GroundSpritePath_(rAppearance, rTile);
        if (!ground.empty() && TryDrawTileSprite_(rGraphics, ground, rTerrain))
        {
            return;
        }
    }
    const TileShape_t overlay =
        (layer == ArtLayer_t::Landmark || layer == ArtLayer_t::Vegetation)
            ? OverlayShape_(rShape, rTile, rAppearance.CoverOf(rTile))
            : rTerrain;
    rAppearance.OccupantsOf(rTile).ForEach([&](const ImprovementConfig_t& rOccupant) {
        if (!HasLayerArt_(rOccupant, layer))
        {
            return false;
        }
        // Missing terrain art draws nothing; the fill underneath remains.
        if (layer == ArtLayer_t::Landform && rTile.IsWater())
        {
            (void)TryDrawWaterLandform_(rGraphics, rTile, rOccupant, rAppearance, rShape);
        }
        else
        {
            (void)TryDrawLayerSprite_(rGraphics, rTile, rOccupant, rAppearance, overlay);
        }
        return false;
    });
}

void TileRenderer::RenderTerrain(Graphics& rGraphics, const Tile& rTile, const TileShape_t& rShape,
                                 const MapAppearance& rAppearance) const
{
    rGraphics.FillTileShape(rShape, FillColor(rTile, rAppearance));
    const TileCover_t cover = rAppearance.CoverOf(rTile);
    if (cover == TileCover_t::Shroud)
    {
        return;
    }
    const TileShape_t terrain = TerrainShape_(rShape, rTile, cover);

    constexpr std::array<ArtLayer_t, 5> k_BeforeCoast = {
        ArtLayer_t::Landform,   ArtLayer_t::Moisture,   ArtLayer_t::Rockiness,
        ArtLayer_t::Landmark,   ArtLayer_t::Vegetation,
    };
    for (const ArtLayer_t layer : k_BeforeCoast)
    {
        DrawTerrainLayer_(rGraphics, rTile, layer, rAppearance, rShape, terrain);
    }

    // The coast covers the terrain layers and sits under rivers, roads and improvements.
    // TODO: confirm in terranx.exe whether remembered tiles keep their old elevation
    DrawCoastOverlay_(rGraphics, rTile, rAppearance.Map(), rShape,
                      cover == TileCover_t::Fog ? m_rStyle.fogLandShade : 0.0f);

    DrawTerrainLayer_(rGraphics, rTile, ArtLayer_t::River, rAppearance, rShape, terrain);
    DrawLinkNetworks_(rGraphics, rTile, rAppearance, terrain);

    // Fog hazes the terrain layers; objects draw clear on top of it, as in SMAC.
    if (cover == TileCover_t::Fog)
    {
        rGraphics.FillTileShape(rShape, m_rStyle.fogHazeColor);
    }
}

void TileRenderer::RenderObjects(Graphics& rGraphics, const Tile& rTile, const TileShape_t& rShape,
                                 const MapAppearance& rAppearance,
                                 const YieldLookup_t& rYieldOf) const
{
    if (rAppearance.CoverOf(rTile) == TileCover_t::Shroud)
    {
        return;
    }
    const TileOccupants_t occupants = rAppearance.OccupantsOf(rTile);
    std::vector<std::string> hidden;
    occupants.ForEach([&hidden](const ImprovementConfig_t& rOccupant) {
        if (rOccupant.art)
        {
            hidden.insert(hidden.end(), rOccupant.art->hides.begin(), rOccupant.art->hides.end());
        }
        return false;
    });

    // Terrain bonuses and the Monolith come before improvements, as SMAC draws them.
    occupants.ForEach([&](const ImprovementConfig_t& rOccupant) {
        if (!HasLayerArt_(rOccupant, ArtLayer_t::Object)
            || std::ranges::find(hidden, rOccupant.id) != hidden.end())
        {
            return false;
        }
        const std::string path = ObjectSpritePath_(rOccupant, rTile, rYieldOf);
        if (!path.empty())
        {
            DrawObject_(rGraphics, rOccupant, path, rShape);
        }
        return false;
    });
}

} // namespace ac
