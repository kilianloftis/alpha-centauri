#include "ui/TileRenderer.h"

#include "game/map/Tile.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

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

} // namespace

void TileRenderer::Render(Graphics& rGraphics, const Tile& rTile, float x, float y, float size,
                          bool bFogged)
{
    const auto& s = Style().tileRenderer;
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
