#include "ui/TileRenderer.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementIds.h"
#include "game/map/Tile.h"
#include "game/map/TileLayer.h"
#include "game/map/TileLayerResolver.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace ac
{

namespace
{

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
bool TryDrawSprite_(Graphics& rGraphics, const std::string& path, float x, float y, float size,
                    const Color_t& tint)
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

    return rGraphics.DrawSprite(path, x, y, size, size, tint);
}

bool TryDrawLayerSprite_(Graphics& rGraphics, const Tile& rTile, const std::string& contentId,
                         float x, float y, float size, const Color_t& tint)
{
    // Improvement layer already returns PascalCase config ids; other layers use TileLayerContent.
    const bool bLooksLikeConfigId =
        !contentId.empty() && std::isupper(static_cast<unsigned char>(contentId.front()));
    const std::string configId = bLooksLikeConfigId ? contentId : ContentIdToConfigId_(contentId);
    const ImprovementConfig_t* pOccupant = FindOccupantByConfigId_(rTile, configId);
    if (!pOccupant)
    {
        return false;
    }
    return TryDrawSprite_(rGraphics, pOccupant->spritePath, x, y, size, tint);
}

void DrawInsetRect_(Graphics& rGraphics, float x, float y, float size, float insetRatio,
                    const Color_t& color)
{
    const float inset = size * insetRatio;
    const float span = size - 2.0f * inset;
    if (span <= 0.0f)
    {
        return;
    }
    rGraphics.DrawFilledRect(x + inset, y + inset, span, span, color);
}

void DrawRockinessRing_(Graphics& rGraphics, float x, float y, float size, const Color_t& ringColor,
                        const Color_t& holeColor, float outerInsetRatio, float innerInsetRatio)
{
    DrawInsetRect_(rGraphics, x, y, size, outerInsetRatio, ringColor);
    DrawInsetRect_(rGraphics, x, y, size, innerInsetRatio, holeColor);
}

bool ShouldSkipLandProceduralOverlays_(const Tile& rTile)
{
    return !rTile.IsLand() || rTile.HasFeature(ImprovementIds::k_Fungus)
           || rTile.HasImprovement(ImprovementIds::k_Forest);
}

void DrawProceduralRockiness_(Graphics& rGraphics, const Tile& rTile, float x, float y, float size,
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
    DrawRockinessRing_(rGraphics, x, y, size, ring, baseFill, s.landformRingOuterInsetRatio,
                       s.landformRingInnerInsetRatio);
}

void DrawProceduralMoisture_(Graphics& rGraphics, const Tile& rTile, float x, float y, float size,
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
    DrawInsetRect_(rGraphics, x, y, size, s.landformRingInnerInsetRatio, center);
}

} // namespace

Color_t TileRenderer::FillColor(const Tile& rTile, bool bFogged)
{
    const auto& s = Style().tileRenderer;
    const int elevation = rTile.GetElevation();
    Color_t fill{};

    // Feature overlays win over the elevation gradient (Forest excludes Fungus in config).
    if (rTile.HasFeature(ImprovementIds::k_Fungus))
    {
        fill = s.fungusColor;
    }
    else if (rTile.HasImprovement(ImprovementIds::k_Forest))
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
                          bool bFogged)
{
    const auto& s = Style().tileRenderer;
    const Color_t baseFill = FillColor(rTile, bFogged);
    const Color_t tint = SpriteTint_(rTile, bFogged);

    rGraphics.DrawFilledRect(x, y, size, size, baseFill);

    for (const TileLayer_t& rLayer : ResolveTileLayers(rTile))
    {
        if (!rLayer.contentId.has_value())
        {
            continue;
        }
        if (TryDrawLayerSprite_(rGraphics, rTile, *rLayer.contentId, x, y, size, tint))
        {
            continue;
        }
        // Per-layer procedural cues when that layer's sprite is missing.
        if (rLayer.type == TileLayerType_t::Rockiness
            || (rLayer.type == TileLayerType_t::Landform
                && *rLayer.contentId == TileLayerContent::k_Rolling))
        {
            DrawProceduralRockiness_(rGraphics, rTile, x, y, size, bFogged, baseFill);
        }
        else if (rLayer.type == TileLayerType_t::Moisture)
        {
            DrawProceduralMoisture_(rGraphics, rTile, x, y, size, bFogged);
        }
    }

    // Improvements already drawn via the Improvement layer when they are the dominant occupant.
    // Also draw any remaining improvement sprites that carry art (e.g. tile bonuses on terrain).
    for (const ImprovementConfig_t* pImprovement : rTile.GetImprovements())
    {
        if (!pImprovement || pImprovement->spritePath.empty())
        {
            continue;
        }
        if (pImprovement->id == ImprovementIds::k_Farm || pImprovement->id == ImprovementIds::k_Forest
            || pImprovement->id == ImprovementIds::k_Road)
        {
            continue;
        }
        (void)TryDrawSprite_(rGraphics, pImprovement->spritePath, x, y, size, tint);
    }

    // Optional terrain bonuses / monolith sit in GetTerrainFeatures, not improvements.
    for (const ImprovementConfig_t* pFeature : rTile.GetTerrainFeatures())
    {
        if (!pFeature || pFeature->spritePath.empty())
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
        (void)TryDrawSprite_(rGraphics, pFeature->spritePath, x, y, size, tint);
    }

    rGraphics.DrawRect(x, y, size, size, s.tileBorderColor, s.tileBorderWidth);
}

} // namespace ac
