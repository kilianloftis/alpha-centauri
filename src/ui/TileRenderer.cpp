#include "ui/TileRenderer.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementIds.h"
#include "game/map/Tile.h"
#include "game/map/TileLayer.h"
#include "game/map/TileLayerResolver.h"
#include "game/map/WorldMap.h"
#include "graphics/Graphics.h"
#include "ui/TileSpriteEdgeInset.h"
#include "ui/style/UiStyle.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

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

// Soft elevation/fog multiply: blend white toward the procedural fill so texture detail remains.
Color_t SpriteTint_(const Tile& rTile, bool bFogged)
{
    constexpr float k_ElevationTintBlend = 0.4f;
    const Color_t fill = TileRenderer::FillColor(rTile, /*bFogged*/ false);
    Color_t tint = LerpColor_(Color_t::White(), fill, k_ElevationTintBlend);
    if (bFogged)
    {
        tint = DimColor_(tint, Style().tileRenderer.fogFillDimRatio);
    }
    return tint;
}

// True when a sprite was drawn. Empty path, missing file, or load/draw failure → false so the
// caller can paint the procedural fallback.
bool TryDrawSprite_(Graphics& rGraphics, const std::string& path, float x, float y, float width,
                    float height, const Color_t& tint)
{
    if (path.empty())
    {
        return false;
    }

    SpriteCacheState_t& rState = SpriteCache_()[path];
    if (rState == SpriteCacheState_t::Missing)
    {
        return false;
    }
    if (rState == SpriteCacheState_t::Untried)
    {
        if (!std::filesystem::exists(path) || !rGraphics.LoadTexture(path, path))
        {
            rState = SpriteCacheState_t::Missing;
            return false;
        }
        rState = SpriteCacheState_t::Loaded;
    }

    return rGraphics.DrawSprite(path, x, y, width, height, tint);
}

bool TryDrawOccupantSprite_(Graphics& rGraphics, const Tile& rTile, std::string_view configId,
                            float x, float y, float width, float height, const Color_t& tint)
{
    const ImprovementConfig_t* pOccupant = rTile.FindOccupantConfig(configId);
    if (!pOccupant)
    {
        pOccupant = FindOccupantByConfigId_(rTile, configId);
    }
    if (!pOccupant)
    {
        return false;
    }
    const std::string& path =
        PickSpritePath(pOccupant->spritePaths, rTile.GetX(), rTile.GetY(), pOccupant->id);
    return TryDrawSprite_(rGraphics, path, x, y, width, height, tint);
}

bool TryDrawLayerSprite_(Graphics& rGraphics, const Tile& rTile, const std::string& contentId,
                         float x, float y, float width, float height, const Color_t& tint)
{
    // Improvement layer already returns PascalCase config ids; other layers use TileLayerContent.
    const bool bLooksLikeConfigId =
        !contentId.empty() && std::isupper(static_cast<unsigned char>(contentId.front()));
    const std::string configId = bLooksLikeConfigId ? contentId : ContentIdToConfigId_(contentId);
    return TryDrawOccupantSprite_(rGraphics, rTile, configId, x, y, width, height, tint);
}

// Land rainfall blends by stacking arid → moist → wet. Each tier insets where ortho neighbors
// are below that tier, so drier bases show through at moisture boundaries.
bool DrawMoistureStack_(Graphics& rGraphics, const Tile& rTile, float x, float y, float size,
                        const Color_t& tint, const WorldMap* pMap)
{
    if (!rTile.IsLand())
    {
        return false;
    }

    const float insetRatio = Style().tileRenderer.spriteEdgeInsetRatio;
    const Moisture_t moisture = rTile.GetMoisture();
    bool bDrewAny = false;

    auto drawTier = [&](Moisture_t tier, std::string_view configId) {
        if (static_cast<int>(moisture) < static_cast<int>(tier))
        {
            return;
        }
        const SpriteEdgeMatch_t match = MatchMoistureTierEdges(rTile, pMap, tier);
        const SpriteDestRect_t dest = DestRectForEdgeInsets(x, y, size, match, insetRatio);
        if (TryDrawOccupantSprite_(rGraphics, rTile, configId, dest.x, dest.y, dest.width,
                                   dest.height, tint))
        {
            bDrewAny = true;
        }
    };

    drawTier(Moisture_t::Arid, "Arid");
    drawTier(Moisture_t::Moist, "Moist");
    drawTier(Moisture_t::Wet, "Wet");
    return bDrewAny;
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
    const Color_t tint = SpriteTint_(rTile, bFogged);
    // Fungus art is already pink-remapped; elevation tint would turn it into muddy splotches.
    Color_t fungusTint = Color_t::White();
    if (bFogged)
    {
        fungusTint = DimColor_(fungusTint, s.fogFillDimRatio);
    }

    rGraphics.DrawFilledDiamond(x, y, width, height, baseFill);

    for (const TileLayer_t& rLayer : ResolveTileLayers(rTile))
    {
        if (!rLayer.contentId.has_value())
        {
            continue;
        }
        const bool bFungusLayer = rLayer.type == TileLayerType_t::Vegetation
                                  && *rLayer.contentId == TileLayerContent::k_Fungus;
        const Color_t& rLayerTint = bFungusLayer ? fungusTint : tint;

        if (rLayer.type == TileLayerType_t::Moisture)
        {
            if (!DrawMoistureStack_(rGraphics, rTile, x, y, size, rLayerTint, pMap))
            {
                DrawProceduralMoisture_(rGraphics, rTile, x, y, width, height, bFogged);
            }
            continue;
        }

        SpriteEdgeMatch_t edgeMatch{};
        float edgeInsetRatio = s.spriteEdgeInsetRatio;
        bool bApplyEdgeInset = false;
        if (rLayer.type == TileLayerType_t::Rockiness)
        {
            edgeMatch = MatchRockinessEdges(rTile, pMap);
            edgeInsetRatio = s.spriteOverlayEdgeInsetRatio;
            bApplyEdgeInset = true;
        }
        else if (bFungusLayer)
        {
            edgeMatch = MatchFungusEdges(rTile, pMap);
            edgeInsetRatio = s.spriteOverlayEdgeInsetRatio;
            bApplyEdgeInset = true;
        }
        else if (rLayer.type == TileLayerType_t::Landform && rTile.IsWater())
        {
            edgeMatch = MatchSeaLandformEdges(rTile, pMap, *rLayer.contentId);
            bApplyEdgeInset = true;
        }

        const SpriteDestRect_t dest =
            bApplyEdgeInset ? DestRectForEdgeInsets(x, y, size, edgeMatch, edgeInsetRatio)
                            : SpriteDestRect_t{x, y, width, height};

        if (TryDrawLayerSprite_(rGraphics, rTile, *rLayer.contentId, dest.x, dest.y, dest.width,
                                dest.height, rLayerTint))
        {
            continue;
        }
        // Per-layer procedural cues when that layer's sprite is missing.
        if (bFungusLayer)
        {
            rGraphics.DrawFilledDiamond(dest.x, dest.y, dest.width, dest.height, s.fungusColor);
        }
        else if (rLayer.type == TileLayerType_t::Rockiness)
        {
            DrawProceduralRockiness_(rGraphics, rTile, dest.x, dest.y, dest.width, dest.height,
                                     bFogged, baseFill);
        }
    }

    // Improvements already drawn via the Improvement layer when they are the dominant occupant.
    // Also draw any remaining improvement sprites that carry art (e.g. tile bonuses on terrain).
    for (const ImprovementConfig_t* pImprovement : rTile.GetImprovements())
    {
        if (!pImprovement || pImprovement->spritePaths.empty())
        {
            continue;
        }
        if (pImprovement->id == ImprovementIds::k_Farm || pImprovement->id == ImprovementIds::k_Forest
            || pImprovement->id == ImprovementIds::k_Road)
        {
            continue;
        }
        const std::string& path = PickSpritePath(pImprovement->spritePaths, rTile.GetX(),
                                                 rTile.GetY(), pImprovement->id);
        (void)TryDrawSprite_(rGraphics, path, x, y, width, height, tint);
    }

    // Optional terrain bonuses / monolith sit in GetTerrainFeatures, not improvements.
    for (const ImprovementConfig_t* pFeature : rTile.GetTerrainFeatures())
    {
        if (!pFeature || pFeature->spritePaths.empty())
        {
            continue;
        }
        // Axes (Flat/Moist/…) and water bands are drawn via layers; skip duplicates.
        if (pFeature->id == "Flat" || pFeature->id == "Rolling" || pFeature->id == "Rocky"
            || pFeature->id == "Arid" || pFeature->id == "Moist" || pFeature->id == "Wet"
            || pFeature->id == "Water" || pFeature->id == "Ocean" || pFeature->id == "OceanShelf"
            || pFeature->id == "Fungus" || pFeature->id == "River")
        {
            continue;
        }
        const std::string& path =
            PickSpritePath(pFeature->spritePaths, rTile.GetX(), rTile.GetY(), pFeature->id);
        (void)TryDrawSprite_(rGraphics, path, x, y, width, height, tint);
    }

    rGraphics.DrawDiamond(x, y, width, height, s.tileBorderColor, s.tileBorderWidth);
}

} // namespace ac
