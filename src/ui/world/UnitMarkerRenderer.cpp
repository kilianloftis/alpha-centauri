#include "ui/world/UnitMarkerRenderer.h"

#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/UnitVisibility.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/UnitDesign.h"
#include "ui/style/UiStyle.h"
#include "ui/world/MapViewport.h"

namespace ac
{

namespace
{

constexpr size_t k_UnitNameFirstCharCount    = 1;

} // namespace

void UnitMarkerRenderer::Render(Graphics& rGraphics,
                                const GameState& rGameState,
                                const MapViewport& rViewport)
{
    m_markerRects.clear();

    const auto& s = Style().unitMarker;
    const WorldMap& rWorldMap = rGameState.GetWorldMap();
    const float tileSize = rViewport.TileSize();
    const float markerWidth = tileSize * s.widthRatio;
    const float markerHeight = tileSize * s.heightRatio;
    const float spacing = tileSize * s.spacingRatio;
    const Faction* pPlayer = rGameState.GetPlayerFaction();

    rViewport.ForEachVisibleTile([&](const Tile& rTile, float tileX, float tileY) {
        const std::vector<Unit*>& units = rWorldMap.GetUnitsOnTile(rTile);
        if (units.empty())
        {
            return;
        }

        // Per-unit visibility (fog, Conceal/Detect, and contact reveal) — not tile fog
        // alone, so contact-revealed units still draw even if they pierce fog of war.
        size_t drawn = 0;
        for (const Unit* pUnit : units)
        {
            if (!pUnit)
            {
                continue;
            }
            if (pPlayer && !IsUnitVisibleTo(*pPlayer, *pUnit, rGameState.GetTileEffects()))
            {
                continue;
            }

            const Rectangle_t marker{
                tileX + spacing + (drawn * (markerWidth + spacing)),
                tileY + tileSize - markerHeight - spacing,
                markerWidth,
                markerHeight};
            ++drawn;

            m_markerRects[pUnit->GetUnitId()] = marker;
            DrawMarker(rGraphics, *pUnit, marker, pUnit == m_pSelectedUnit);
        }
    });
}

std::optional<Rectangle_t> UnitMarkerRenderer::GetCachedMarkerRect(UnitId_t unitId) const
{
    const auto it = m_markerRects.find(unitId);
    if (it == m_markerRects.end())
    {
        return std::nullopt;
    }
    return it->second;
}

Rectangle_t UnitMarkerRenderer::MarkerRectOnTile(float tileX, float tileY, float tileSize)
{
    const auto& s = Style().unitMarker;
    const float markerWidth = tileSize * s.widthRatio;
    const float markerHeight = tileSize * s.heightRatio;
    const float spacing = tileSize * s.spacingRatio;
    return Rectangle_t{
        tileX + spacing,
        tileY + tileSize - markerHeight - spacing,
        markerWidth,
        markerHeight};
}

void UnitMarkerRenderer::DrawMarker(Graphics& rGraphics, const Unit& rUnit,
                                    const Rectangle_t& rMarker, bool bSelected)
{
    const auto& s = Style().unitMarker;
    const bool bExhausted = rUnit.GetMoveFragmentsRemaining() <= 0;
    const Color_t& markerColor = bExhausted ? s.exhaustedColor : s.markerColor;

    // TODO: Use faction color based on rUnit.GetFaction().
    rGraphics.DrawFilledRect(rMarker.x, rMarker.y, rMarker.width, rMarker.height, markerColor);

    if (bSelected)
    {
        rGraphics.DrawRect(
            rMarker.x - s.selectionBorderOffset,
            rMarker.y - s.selectionBorderOffset,
            rMarker.width + s.selectionBorderExpansion,
            rMarker.height + s.selectionBorderExpansion,
            s.selectionBorderColor,
            s.selectionBorderWidth);
    }

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

void UnitMarkerRenderer::DrawHitOverlay(Graphics& rGraphics, const Rectangle_t& rMarker)
{
    const auto& s = Style().unitMarker;
    // Placeholder: a translucent red plate over the unit marker. Swap for a hit animation.
    rGraphics.DrawFilledRect(rMarker.x, rMarker.y, rMarker.width, rMarker.height, s.hitOverlayFill);
    rGraphics.DrawRect(rMarker.x, rMarker.y, rMarker.width, rMarker.height, s.hitOverlayBorder,
                       s.hitOverlayBorderWidth);
}

} // namespace ac
