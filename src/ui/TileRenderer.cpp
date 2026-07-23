#include "ui/TileRenderer.h"

#include "game/map/Tile.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace ac
{

namespace
{

constexpr int   k_MoistureWetValue       = 2;
constexpr int   k_MoistureMoistValue     = 1;
constexpr int   k_MoistureAridValue      = 0;
constexpr int   k_RockinessRockyValue    = 2;
constexpr int   k_RockinessRollingValue  = 1;
constexpr int   k_RockinessFlatValue     = 0;
constexpr int   k_ElevationMetersPerKm   = 1000;

int MoistureToInt_(Moisture_t moisture)
{
    switch (moisture)
    {
        case Moisture_t::Wet:
            return k_MoistureWetValue;
        case Moisture_t::Moist:
            return k_MoistureMoistValue;
        case Moisture_t::Arid:
        default:
            return k_MoistureAridValue;
    }
}

int RockinessToInt_(Rockiness_t rockiness)
{
    switch (rockiness)
    {
        case Rockiness_t::Rocky:
            return k_RockinessRockyValue;
        case Rockiness_t::Rolling:
            return k_RockinessRollingValue;
        case Rockiness_t::Flat:
        default:
            return k_RockinessFlatValue;
    }
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

} // namespace

Color_t TileRenderer::FillColor(const Tile& rTile, bool bFogged)
{
    const auto& s = Style().tileRenderer;
    const int elevation = rTile.GetElevation();
    Color_t fill{};

    if (rTile.IsWater())
    {
        // Water: darker at depth (minElevation), lighter near sea level (-1).
        const float t = Remap01_(static_cast<float>(elevation),
                                 static_cast<float>(s.minElevationMeters),
                                 -1.0f);
        fill = LerpColor_(s.waterLowColor, s.waterHighColor, t);
    }
    else
    {
        // Land: darker near sea level (0), lighter at peaks (maxElevation).
        const float t = Remap01_(static_cast<float>(elevation),
                                 0.0f,
                                 static_cast<float>(s.maxElevationMeters));
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

    rGraphics.DrawFilledRect(x, y, size, size, FillColor(rTile, bFogged));
    rGraphics.DrawRect(x, y, size, size, s.tileBorderColor, s.tileBorderWidth);

    const int moisture = MoistureToInt_(rTile.GetMoisture());
    const int rockiness = RockinessToInt_(rTile.GetRockiness());
    const int elevationKm = rTile.GetElevation() / k_ElevationMetersPerKm;

    std::ostringstream oss;
    oss << moisture << " " << rockiness << " " << elevationKm;

    const Color_t textColor = bFogged ? s.fogTerrainColor : s.clearTerrainTextColor;
    rGraphics.DrawText(
        oss.str(),
        x + size * s.tileTextOffsetXRatio,
        y + size * s.tileTextOffsetYRatio,
        s.tileFontSize,
        textColor);
}

} // namespace ac
