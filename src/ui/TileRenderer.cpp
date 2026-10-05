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
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
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

// Terrain art keeps its sheet colours. Fogged land is dimmed, as SMAC shades remembered land
// two palette steps darker; water keeps its depth shading.
Color_t TerrainTint_(const Tile& rTile, bool bFogged)
{
    if (bFogged && rTile.IsLand())
    {
        return DimColor_(Color_t::White(), Style().tileRenderer.fogTerrainDimRatio);
    }
    return Color_t::White();
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

bool TryDrawDiamondSprite_(Graphics& rGraphics, const std::string& path, float x, float y,
                           float width, float height, const DiamondTint_t& rTint)
{
    return EnsureSpriteLoaded_(rGraphics, path)
           && rGraphics.DrawDiamondSprite(path, x, y, width, height, rTint);
}

DiamondTint_t UniformTint_(const Color_t& tint)
{
    return DiamondTint_t{tint, tint, tint, tint, tint};
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
// their shared corners; the shore takes the land's terrain tint.
void DrawCoastOverlay_(Graphics& rGraphics, const Tile& rTile, const WorldMap& rMap, float x,
                       float y, float width, float height, const Color_t& shoreTint)
{
    const CoastOverlay_t overlay = ResolveCoastOverlay(rTile, rMap);
    if (std::ranges::none_of(overlay.corners,
                             [](const CoastCornerArt_t& rArt) { return rArt.waterMask != 0; }))
    {
        return;
    }
    const WaterShadingStyle_t& rShading = Style().tileRenderer.waterShading;
    const DiamondTint_t waterTint = WaterShadeTint(
        ResolveWaterShades(rTile, &rMap, rShading.depthShades), rShading.tints.at(rShading.coastTints));
    for (const CoastCornerArt_t& rArt : overlay.corners)
    {
        if (rArt.waterMask != 0)
        {
            (void)TryDrawDiamondSprite_(rGraphics, CoastSpritePath_("water", rArt), x, y, width,
                                        height, waterTint);
        }
    }
    for (const CoastCornerArt_t& rArt : overlay.corners)
    {
        if (rArt.waterMask != 0)
        {
            (void)TryDrawDiamondSprite_(rGraphics, CoastSpritePath_("shore", rArt), x, y, width,
                                        height, UniformTint_(shoreTint));
        }
    }
}

const std::vector<std::string>& SurfaceSpritePaths_(const ImprovementConfig_t& rOccupant,
                                                    const Tile& rTile)
{
    return rTile.IsWater() ? rOccupant.spritePaths.sea : rOccupant.spritePaths.land;
}

// Object sprites stand on the tile's footprint and reach above it by their overhang ratio.
bool TryDrawOccupantPath_(Graphics& rGraphics, const ImprovementConfig_t& rOccupant,
                          const std::string& path, float x, float y, float width, float height,
                          const Color_t& tint)
{
    const float overhang = height * rOccupant.spriteOverhangRatio;
    return TryDrawSprite_(rGraphics, path, x, y - overhang, width, height + overhang, tint);
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

// The Improvement layer holds object sprites that stand on the footprint; every other layer is
// a baked terrain diamond that meets its neighbors edge to edge.
bool TryDrawLayerSprite_(Graphics& rGraphics, const Tile& rTile, const TileLayer_t& rLayer,
                         const WorldMap* pMap, float x, float y, float width, float height,
                         const Color_t& tint)
{
    const ImprovementConfig_t* pOccupant = FindLayerOccupant_(rTile, rLayer);
    if (!pOccupant)
    {
        return false;
    }
    const std::string path = LayerSpritePath_(rTile, rLayer, *pOccupant, pMap);
    if (rLayer.type == TileLayerType_t::Improvement)
    {
        return TryDrawOccupantPath_(rGraphics, *pOccupant, path, x, y, width, height, tint);
    }
    return TryDrawDiamondSprite_(rGraphics, path, x, y, width, height, UniformTint_(tint));
}

// Water art shaded per vertex by depth with the tints table named by the landform's id; a
// landform without one draws untinted.
bool TryDrawWaterLandform_(Graphics& rGraphics, const Tile& rTile, const TileLayer_t& rLayer,
                           const WorldMap* pMap, float x, float y, float width, float height)
{
    const ImprovementConfig_t* pOccupant = FindLayerOccupant_(rTile, rLayer);
    if (!pOccupant)
    {
        return false;
    }
    const WaterShadingStyle_t& rShading = Style().tileRenderer.waterShading;
    const auto it = rShading.tints.find(pOccupant->id);
    const DiamondTint_t tint =
        it == rShading.tints.end()
            ? DiamondTint_t{}
            : WaterShadeTint(ResolveWaterShades(rTile, pMap, rShading.depthShades), it->second);
    return TryDrawDiamondSprite_(rGraphics, LayerSpritePath_(rTile, rLayer, *pOccupant, pMap), x,
                                 y, width, height, tint);
}

// Missing river art: a line from the tile centre to each connected edge's midpoint, or a short
// cross on a river tile with no river neighbor.
void DrawProceduralRiver_(Graphics& rGraphics, const Tile& rTile, const WorldMap* pMap, float x,
                          float y, float width, float height, bool bFogged)
{
    const auto& s = Style().tileRenderer;
    const Color_t color = bFogged ? DimColor_(s.riverColor, s.fogFillDimRatio) : s.riverColor;
    const float thickness = std::max(1.0f, width * s.riverLineThicknessRatio);
    const float centerX = x + width * 0.5f;
    const float centerY = y + height * 0.5f;
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
        float edgeX;
        float edgeY;
    } k_Edges[] = {
        {RiverConnection_t::North, 0.75f, 0.25f},
        {RiverConnection_t::East, 0.75f, 0.75f},
        {RiverConnection_t::South, 0.25f, 0.75f},
        {RiverConnection_t::West, 0.25f, 0.25f},
    };
    for (const auto& rEdge : k_Edges)
    {
        if (HasRiverConnection(connections, rEdge.direction))
        {
            rGraphics.DrawLine(centerX, centerY, x + width * rEdge.edgeX, y + height * rEdge.edgeY,
                               color, thickness);
        }
    }
}

void DrawInsetDiamond_(Graphics& rGraphics, float x, float y, float width, float height,
                       float insetRatio, const Color_t& color)
{
    const float insetX = width * insetRatio;
    const float insetY = height * insetRatio;
    const float spanW = width - 2.0f * insetX;
    const float spanH = height - 2.0f * insetY;
    if (spanW <= 0.0f || spanH <= 0.0f)
    {
        return;
    }
    rGraphics.DrawFilledDiamond(x + insetX, y + insetY, spanW, spanH, color);
}

void DrawRockinessRing_(Graphics& rGraphics, float x, float y, float width, float height,
                        const Color_t& ringColor, const Color_t& holeColor, float outerInsetRatio,
                        float innerInsetRatio)
{
    DrawInsetDiamond_(rGraphics, x, y, width, height, outerInsetRatio, ringColor);
    DrawInsetDiamond_(rGraphics, x, y, width, height, innerInsetRatio, holeColor);
}

bool ShouldSkipLandProceduralOverlays_(const Tile& rTile)
{
    return !rTile.IsLand() || rTile.HasFeature(ImprovementIds::k_Fungus)
           || rTile.HasImprovement(ImprovementIds::k_Forest);
}

void DrawProceduralRockiness_(Graphics& rGraphics, const Tile& rTile, float x, float y, float width,
                              float height, bool bFogged, const Color_t& baseFill)
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
    DrawRockinessRing_(rGraphics, x, y, width, height, ring, baseFill, s.landformRingOuterInsetRatio,
                       s.landformRingInnerInsetRatio);
}

void DrawProceduralMoisture_(Graphics& rGraphics, const Tile& rTile, float x, float y, float width,
                             float height, bool bFogged)
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
    DrawInsetDiamond_(rGraphics, x, y, width, height, s.landformRingInnerInsetRatio, center);
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

void TileRenderer::Render(Graphics& rGraphics, const Tile& rTile, float x, float y, float size,
                          bool bFogged, const WorldMap* pMap)
{
    const auto& s = Style().tileRenderer;
    const float width = size;
    const float height = size * k_IsoHeightRatio;
    const Color_t baseFill = FillColor(rTile, bFogged);
    const Color_t terrainTint = TerrainTint_(rTile, bFogged);

    rGraphics.DrawFilledDiamond(x, y, width, height, baseFill);

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
            DrawCoastOverlay_(rGraphics, rTile, *pMap, x, y, width, height, terrainTint);
        }
    };
    // Fog hazes the terrain layers; objects draw clear on top of it, as in SMAC.
    bool bHazeDrawn = false;
    auto drawHaze = [&]() {
        if (bHazeDrawn)
        {
            return;
        }
        bHazeDrawn = true;
        if (bFogged)
        {
            rGraphics.DrawFilledDiamond(x, y, width, height, s.fogHazeColor);
        }
    };

    for (const TileLayer_t& rLayer : ResolveTileLayers(rTile))
    {
        if (rLayer.type > TileLayerType_t::Vegetation)
        {
            drawCoast();
        }
        if (rLayer.type == TileLayerType_t::Improvement)
        {
            drawHaze();
        }
        if (!rLayer.contentId.has_value())
        {
            continue;
        }
        if (rLayer.type == TileLayerType_t::Landform && rTile.IsWater())
        {
            (void)TryDrawWaterLandform_(rGraphics, rTile, rLayer, pMap, x, y, width, height);
            continue;
        }
        const bool bFungusLayer = rLayer.type == TileLayerType_t::Vegetation
                                  && *rLayer.contentId == TileLayerContent::k_Fungus;
        const Color_t layerTint =
            rLayer.type == TileLayerType_t::Improvement ? Color_t::White() : terrainTint;

        const SpriteDestRect_t dest =
            rLayer.type == TileLayerType_t::Rockiness
                ? DestRectForEdgeInsets(x, y, size, MatchRockinessEdges(rTile, pMap),
                                        s.spriteOverlayEdgeInsetRatio)
                : SpriteDestRect_t{x, y, width, height};

        if (TryDrawLayerSprite_(rGraphics, rTile, rLayer, pMap, dest.x, dest.y, dest.width,
                                dest.height, layerTint))
        {
            continue;
        }
        // Per-layer procedural cues when that layer's sprite is missing.
        if (rLayer.type == TileLayerType_t::Moisture)
        {
            DrawProceduralMoisture_(rGraphics, rTile, x, y, width, height, bFogged);
        }
        else if (bFungusLayer)
        {
            rGraphics.DrawFilledDiamond(dest.x, dest.y, dest.width, dest.height, s.fungusColor);
        }
        else if (rLayer.type == TileLayerType_t::Rockiness)
        {
            DrawProceduralRockiness_(rGraphics, rTile, dest.x, dest.y, dest.width, dest.height,
                                     bFogged, baseFill);
        }
        else if (rLayer.type == TileLayerType_t::River)
        {
            DrawProceduralRiver_(rGraphics, rTile, pMap, x, y, width, height, bFogged);
        }
    }
    drawCoast();
    drawHaze();

    // Improvements already drawn via the Improvement layer when they are the dominant occupant.
    // Also draw any remaining improvement sprites that carry art (e.g. tile bonuses on terrain).
    for (const ImprovementConfig_t* pImprovement : rTile.GetImprovements())
    {
        if (!pImprovement || SurfaceSpritePaths_(*pImprovement, rTile).empty())
        {
            continue;
        }
        if (pImprovement->id == ImprovementIds::k_Farm || pImprovement->id == ImprovementIds::k_Forest
            || pImprovement->id == ImprovementIds::k_Road)
        {
            continue;
        }
        (void)TryDrawOccupantPath_(rGraphics, *pImprovement,
                                   VariantSpritePath_(*pImprovement, rTile), x, y, width, height,
                                   Color_t::White());
    }

    // Optional terrain bonuses / monolith sit in GetTerrainFeatures, not improvements.
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
        (void)TryDrawOccupantPath_(rGraphics, *pFeature, VariantSpritePath_(*pFeature, rTile), x,
                                   y, width, height, Color_t::White());
    }

    rGraphics.DrawDiamond(x, y, width, height, s.tileBorderColor, s.tileBorderWidth);
}

} // namespace ac
