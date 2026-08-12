#include "ui/TileRenderer.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementIds.h"
#include "game/map/Tile.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

#include <algorithm>
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

const ImprovementConfig_t* FindTerrainFeature_(const Tile& rTile, std::string_view id)
{
    for (const ImprovementConfig_t* pFeature : rTile.GetTerrainFeatures())
    {
        if (pFeature && pFeature->id == id)
        {
            return pFeature;
        }
    }
    return nullptr;
}

// True when a sprite was drawn. Empty path, missing file, or load/draw failure → false so the
// caller can paint the procedural fallback.
bool TryDrawSprite_(Graphics& rGraphics, const std::string& path, float x, float y)
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

    return rGraphics.DrawSprite(path, x, y);
}

bool TryDrawFeatureSprite_(Graphics& rGraphics, const Tile& rTile, std::string_view featureId,
                           float x, float y)
{
    const ImprovementConfig_t* pFeature = FindTerrainFeature_(rTile, featureId);
    if (!pFeature)
    {
        return false;
    }
    return TryDrawSprite_(rGraphics, pFeature->spritePath, x, y);
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

} // namespace

Color_t TileRenderer::FillColor(const Tile& rTile, bool bFogged)
{
    const auto& s = Style().tileRenderer;
    const int elevation = rTile.GetElevation();
    Color_t fill{};

    // Feature overlays win over the elevation gradient (Forest excludes Fungus in config).
    if (rTile.GetHasFungus())
    {
        fill = s.fungusColor;
    }
    else if (rTile.HasImprovement(ImprovementIds::k_Forest))
    {
        fill = s.forestColor;
    }
    else if (rTile.IsWater())
    {
        // Water: darker at depth, lighter near sea level (-1) — same continuous elevation
        // remap as land, using Planet's elevation clamp.
        const float t = Remap01_(static_cast<float>(elevation),
                                 static_cast<float>(k_MinElevation),
                                 -1.0f);
        fill = LerpColor_(s.waterLowColor, s.waterHighColor, t);
    }
    else
    {
        // Land: darker near sea level (0), lighter at peaks.
        const float t = Remap01_(static_cast<float>(elevation),
                                 0.0f,
                                 static_cast<float>(k_MaxElevation));
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

    rGraphics.DrawFilledRect(x, y, size, size, baseFill);

    // Moisture/rockiness landform cues only on bare land. Water already reads as sea from the
    // blue fill; fungus/forest stand in for vegetation sprites until those assets exist.
    const bool bLandformOverlay = rTile.IsLand()
                                  && !rTile.GetHasFungus()
                                  && !rTile.HasImprovement(ImprovementIds::k_Forest);
    if (bLandformOverlay)
    {
        const float dim = bFogged ? s.fogFillDimRatio : 1.0f;
        const Rockiness_t rockiness = rTile.GetRockiness();
        if (rockiness == Rockiness_t::Rolling || rockiness == Rockiness_t::Rocky)
        {
            const std::string featureId = ToString(rockiness);
            if (!TryDrawFeatureSprite_(rGraphics, rTile, featureId, x, y))
            {
                const Color_t ring = DimColor_(
                    rockiness == Rockiness_t::Rocky ? s.rockyRingColor : s.rollingRingColor, dim);
                DrawRockinessRing_(rGraphics, x, y, size, ring, baseFill,
                                   s.landformRingOuterInsetRatio, s.landformRingInnerInsetRatio);
            }
        }

        const Moisture_t moisture = rTile.GetMoisture();
        if (moisture == Moisture_t::Moist || moisture == Moisture_t::Wet)
        {
            const std::string featureId = ToString(moisture);
            if (!TryDrawFeatureSprite_(rGraphics, rTile, featureId, x, y))
            {
                const Color_t center = DimColor_(
                    moisture == Moisture_t::Wet ? s.wetCenterColor : s.moistCenterColor, dim);
                DrawInsetRect_(rGraphics, x, y, size, s.landformRingInnerInsetRatio, center);
            }
        }
    }

    // Optional sprites for placed improvements (tile bonuses, etc.). Missing assets are skipped.
    for (const ImprovementConfig_t* pImprovement : rTile.GetImprovements())
    {
        if (pImprovement)
        {
            (void)TryDrawSprite_(rGraphics, pImprovement->spritePath, x, y);
        }
    }

    rGraphics.DrawRect(x, y, size, size, s.tileBorderColor, s.tileBorderWidth);
}

} // namespace ac
