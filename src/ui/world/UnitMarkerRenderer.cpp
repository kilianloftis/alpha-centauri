#include "ui/world/UnitMarkerRenderer.h"

#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "ui/TileShapeGeometry.h"
#include "ui/style/UiStyle.h"

namespace ac
{

namespace
{

constexpr size_t k_UnitNameFirstCharCount    = 1;

} // namespace

Rectangle_t UnitMarkerRenderer::MarkerRectOnTile(float tileX, float tileY, float tileWidth,
                                                 std::size_t slot)
{
    const auto& s = Style().unitMarker;
    const float markerWidth = tileWidth * s.widthRatio;
    const float markerHeight = tileWidth * s.heightRatio;
    const float spacing = tileWidth * s.spacingRatio;
    const float tileHeight = tileWidth * k_IsoHeightRatio;
    // Anchor on the diamond center — AABB bottom-left sits outside the tile toward SW.
    return Rectangle_t{
        tileX + (tileWidth - markerWidth) * 0.5f
            + static_cast<float>(slot) * (markerWidth + spacing),
        tileY + (tileHeight - markerHeight) * 0.5f,
        markerWidth,
        markerHeight};
}

void UnitMarkerRenderer::DrawMarker(Graphics& rGraphics, const Unit& rUnit,
                                    const Rectangle_t& rMarker)
{
    const auto& s = Style().unitMarker;
    const bool bExhausted = rUnit.GetMoveFragmentsRemaining() <= 0;
    const Color_t& markerColor = bExhausted ? s.exhaustedColor : s.markerColor;

    // TODO: Use faction color based on rUnit.GetFaction().
    rGraphics.DrawFilledRect(rMarker.x, rMarker.y, rMarker.width, rMarker.height, markerColor);

    const std::string& unitName = rUnit.GetDesign().GetName();
    if (unitName.empty())
    {
        return;
    }

    // Keep the same font/inset proportions as map markers (sized off tile, not chip).
    const float referenceTileSize = rMarker.height / s.heightRatio;
    const unsigned int fontSize =
        static_cast<unsigned int>(referenceTileSize * s.fontSizeRatio);
    const float spacing = referenceTileSize * s.spacingRatio;
    rGraphics.DrawText(
        unitName.substr(0, k_UnitNameFirstCharCount),
        rMarker.x + spacing,
        rMarker.y + spacing,
        fontSize,
        s.initialTextColor);
}

void UnitMarkerRenderer::DrawSelection(Graphics& rGraphics, const Rectangle_t& rMarker)
{
    const auto& s = Style().unitMarker;
    rGraphics.DrawRect(
        rMarker.x - s.selectionBorderOffset,
        rMarker.y - s.selectionBorderOffset,
        rMarker.width + s.selectionBorderExpansion,
        rMarker.height + s.selectionBorderExpansion,
        s.selectionBorderColor,
        s.selectionBorderWidth);
}

void UnitMarkerRenderer::DrawHitOverlay(Graphics& rGraphics, const Rectangle_t& rMarker)
{
    const auto& s = Style().unitMarker;
    // Placeholder: a translucent red plate over the unit marker. Swap for a hit animation.
    rGraphics.DrawFilledRect(rMarker.x, rMarker.y, rMarker.width, rMarker.height, s.hitOverlayFill);
    rGraphics.DrawRect(rMarker.x, rMarker.y, rMarker.width, rMarker.height, s.hitOverlayBorder,
                       s.hitOverlayBorderWidth);
}

} // namespace ac
