#include "ui/world/UnitMarkerRenderer.h"

#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/UnitVisibility.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/UnitDesign.h"

namespace ac
{

namespace
{

constexpr float k_UnitMarkerFontSizeRatio    = 0.2f;
constexpr float k_UnitMarkerWidthRatio       = 0.22f;
constexpr float k_UnitMarkerHeightRatio      = 0.22f;
constexpr float k_UnitMarkerSpacingRatio     = 0.03f;
constexpr Color_t k_UnitMarkerColor            {0, 220, 255, 255};
constexpr Color_t k_UnitMarkerExhaustedColor   {0, 110, 130, 200};
constexpr float k_SelectionBorderOffset      = 1.0f;
constexpr float k_SelectionBorderExpansion   = 2.0f;
constexpr float k_SelectionBorderWidth       = 2.0f;
constexpr size_t k_UnitNameFirstCharCount    = 1;
constexpr Color_t k_HitOverlayFill             {255, 40, 40, 160};
constexpr Color_t k_HitOverlayBorder           {255, 80, 80, 255};
constexpr float k_HitOverlayBorderWidth      = 2.0f;

} // namespace

void UnitMarkerRenderer::Render(Graphics& rGraphics,
                                const GameState& rGameState,
                                WindowLayout_t mapLayout,
                                float tileSize,
                                int colStart,
                                int rowStart,
                                int colEnd,
                                int rowEnd)
{
    m_markerRects.clear();

    const WorldMap& rWorldMap = rGameState.GetWorldMap();
    const float markerWidth = tileSize * k_UnitMarkerWidthRatio;
    const float markerHeight = tileSize * k_UnitMarkerHeightRatio;
    const float spacing = tileSize * k_UnitMarkerSpacingRatio;
    const Faction* pPlayer = rGameState.GetPlayerFaction();

    for (int row = rowStart; row < rowEnd; ++row)
    {
        for (int col = colStart; col < colEnd; ++col)
        {
            const Tile* pTile = rWorldMap.GetTile(col, row);
            if (!pTile)
            {
                continue;
            }

            const std::vector<Unit*>& units = rWorldMap.GetUnitsOnTile(*pTile);
            if (units.empty())
            {
                continue;
            }

            const float tileX = mapLayout.x + ((col - colStart) * tileSize);
            const float tileY = mapLayout.y + ((row - rowStart) * tileSize);

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
        }
    }
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
    const float markerWidth = tileSize * k_UnitMarkerWidthRatio;
    const float markerHeight = tileSize * k_UnitMarkerHeightRatio;
    const float spacing = tileSize * k_UnitMarkerSpacingRatio;
    return Rectangle_t{
        tileX + spacing,
        tileY + tileSize - markerHeight - spacing,
        markerWidth,
        markerHeight};
}

void UnitMarkerRenderer::DrawMarker(Graphics& rGraphics, const Unit& rUnit,
                                    const Rectangle_t& rMarker, bool bSelected)
{
    const bool bExhausted = rUnit.GetMoveFragmentsRemaining() <= 0;
    const Color_t& markerColor = bExhausted ? k_UnitMarkerExhaustedColor : k_UnitMarkerColor;

    // TODO: Use faction color based on rUnit.GetFaction().
    rGraphics.DrawFilledRect(rMarker.x, rMarker.y, rMarker.width, rMarker.height, markerColor);

    if (bSelected)
    {
        rGraphics.DrawRect(
            rMarker.x - k_SelectionBorderOffset,
            rMarker.y - k_SelectionBorderOffset,
            rMarker.width + k_SelectionBorderExpansion,
            rMarker.height + k_SelectionBorderExpansion,
            Color_t::Yellow(),
            k_SelectionBorderWidth);
    }

    const std::string& unitName = rUnit.GetDesign().GetName();
    if (unitName.empty())
    {
        return;
    }

    // Keep the same font/inset proportions as map markers (sized off tile, not chip).
    const float referenceTileSize = rMarker.height / k_UnitMarkerHeightRatio;
    const unsigned int fontSize =
        static_cast<unsigned int>(referenceTileSize * k_UnitMarkerFontSizeRatio);
    const float spacing = referenceTileSize * k_UnitMarkerSpacingRatio;
    rGraphics.DrawText(
        unitName.substr(0, k_UnitNameFirstCharCount),
        rMarker.x + spacing,
        rMarker.y + spacing,
        fontSize,
        Color_t::Black());
}

void UnitMarkerRenderer::DrawHitOverlay(Graphics& rGraphics, const Rectangle_t& rMarker)
{
    // Placeholder: a translucent red plate over the unit marker. Swap for a hit animation.
    rGraphics.DrawFilledRect(rMarker.x, rMarker.y, rMarker.width, rMarker.height, k_HitOverlayFill);
    rGraphics.DrawRect(rMarker.x, rMarker.y, rMarker.width, rMarker.height, k_HitOverlayBorder,
                       k_HitOverlayBorderWidth);
}

} // namespace ac
