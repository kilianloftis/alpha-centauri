#include "ui/world/WorldDisplay.h"
#include "game/GameDataContext.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/effects/TileEffectsContext.h"
#include "game/Faction.h"
#include "game/faction/FactionExploredMap.h"
#include "game/faction/FactionVisibleMap.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Pathfinder.h"
#include "game/units/Unit.h"
#include "ui/style/UiStyle.h"
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace ac
{

namespace
{

constexpr size_t k_BaseNameMinTruncChars = 3;

struct PlayerFogMaps_t
{
    const FactionExploredMap* explored = nullptr;
    const FactionVisibleMap* visible = nullptr;
};

PlayerFogMaps_t PlayerFog_(const GameState& rGameState)
{
    const Faction* pPlayer = rGameState.GetPlayerFaction();
    if (!pPlayer || !pPlayer->GetExploredMap().IsSized() || !pPlayer->GetVisibleMap().IsSized())
    {
        return {};
    }
    return {&pPlayer->GetExploredMap(), &pPlayer->GetVisibleMap()};
}

} // namespace

WorldDisplay::WorldDisplay(const GameState& rGameState, TileRenderer& rTileRenderer,
                           WindowLayout_t layout)
    : m_rGameState(rGameState)
    , m_mapSurface(rTileRenderer, *rGameState.GetGameData().baseSpriteSizes,
                   *rGameState.GetGameData().mapOverlayChannels)
    , m_viewport(rGameState.GetWorldMap(), layout,
                 layout.height * Style().worldDisplay.defaultTileScale)
{
}

void WorldDisplay::SetPathPreview(const Path_t* pPath)
{
    m_pPathPreview = pPath;
}

void WorldDisplay::SetSelectedUnit(const Unit* pUnit)
{
    m_pSelectedUnit = pUnit;
    m_unitMarkers.SetSelectedUnit(pUnit);
}

void WorldDisplay::SetPlaybackVisibleUnits(const std::unordered_set<UnitId_t>* pUnitIds)
{
    m_unitMarkers.SetPlaybackVisibleUnits(pUnitIds);
}

float WorldDisplay::GetEffectiveTileSize() const
{
    return m_viewport.TileSize();
}

int WorldDisplay::GetVisibleRows() const
{
    return m_viewport.VisibleRows();
}

void WorldDisplay::RenderBases_(Graphics& rGraphics)
{
    const auto& s = Style().worldDisplay;
    const float tileSize = m_viewport.TileSize();
    const PlayerFogMaps_t fog = PlayerFog_(m_rGameState);

    const unsigned int fontSize = static_cast<unsigned int>(tileSize * s.baseNameFontSizeRatio);
    const float textOffsetX = tileSize * s.baseTextOffsetRatio;
    const float textOffsetY = tileSize * s.baseTextOffsetRatio;

    // Same visible-wrap pass as unit markers / tile objects so bases seat on the on-screen
    // diamond, not a different wrap of PixelOriginOf.
    m_viewport.ForEachVisibleTile(
        [&](const Tile& rTile, const TileShape_t& rShape) {
            const BaseManager* pBase = m_rGameState.FindBaseAt(rTile.GetX(), rTile.GetY());
            if (!pBase)
            {
                return;
            }
            const BaseManager& rBase = *pBase;
            const Faction& rFaction = rBase.GetFaction();

            // Shroud hides bases entirely; fog still shows last-known bases.
            if (fog.explored && !fog.explored->IsExplored(rTile))
            {
                return;
            }

            Color_t nameColor = s.baseNameColor;
            if (const auto colors = m_mapSurface.ColorsFor(rFaction))
            {
                nameColor = colors->LabelColor(s.baseNameColor);
            }

            const auto [labelX, labelY] = m_viewport.FootprintOrigin(rShape);
            (void)m_mapSurface.RenderBase(rGraphics, rBase, rShape);

            const size_t maxChars = static_cast<size_t>(
                (tileSize * s.baseNameWidthRatio) / (fontSize * s.baseNameCharWidthRatio));
            std::string displayName = rBase.GetName();
            if (displayName.length() > maxChars && maxChars > k_BaseNameMinTruncChars)
            {
                displayName = displayName.substr(0, maxChars - 1) + ".";
            }
            else if (displayName.length() > maxChars)
            {
                displayName = displayName.substr(0, maxChars);
            }

            // TODO: Show capture animation when base capture is implemented
            // TODO: Show population size below name
            rGraphics.DrawText(displayName, labelX + textOffsetX, labelY + textOffsetY, fontSize,
                               nameColor);
        },
        /*bShaded=*/false);
}

void WorldDisplay::RenderPathPreview_(Graphics& rGraphics)
{
    if (!m_pPathPreview || m_pPathPreview->tiles.empty())
    {
        return;
    }

    const auto& s = Style().worldDisplay;

    // Path_t excludes the origin; start from the selected unit so the line begins on its tile.
    std::vector<std::pair<float, float>> centers;
    centers.reserve(m_pPathPreview->tiles.size() + 1);
    if (m_pSelectedUnit)
    {
        if (const auto center = m_viewport.PixelCenterOf(m_pSelectedUnit->GetTile()))
        {
            centers.push_back(*center);
        }
    }
    for (const Tile* pTile : m_pPathPreview->tiles)
    {
        if (!pTile)
        {
            continue;
        }
        if (const auto center = m_viewport.PixelCenterOf(*pTile))
        {
            centers.push_back(*center);
        }
    }

    if (centers.size() < 2)
    {
        return;
    }

    const float thickness = std::max(1.0f, m_viewport.TileSize() * s.pathPreviewLineThicknessRatio);
    for (size_t i = 1; i < centers.size(); ++i)
    {
        rGraphics.DrawLine(centers[i - 1].first, centers[i - 1].second, centers[i].first,
                           centers[i].second, s.pathPreviewColor, thickness);
    }
}

void WorldDisplay::Render(Graphics& rGraphics)
{
    const WorldMap& rWorldMap = m_viewport.GetWorldMap();
    if (rWorldMap.GetWidth() <= 0 || rWorldMap.GetHeight() <= 0)
    {
        return;
    }

    const MapDisplayConfig_t& rDisplay = m_rGameState.GetSettings().GetMapDisplay();
    m_viewport.SetRelief(rDisplay.relief, Style().tileRenderer.relief);
    const PlayerFogMaps_t fog = PlayerFog_(m_rGameState);
    const MapSurfaceRenderer::YieldLookup_t yieldOf = [this](const Tile& rTile) {
        return m_rGameState.GetTileEffects().ResolveTileYield(rTile).effective;
    };
    const MapSurfaceRenderer::ExploredFn_t explored = [&fog](const Tile& rTile) {
        return !fog.explored || fog.explored->IsExplored(rTile);
    };

    m_viewport.ForEachVisibleTile([&](const Tile& rTile, const TileShape_t& rShape) {
        const bool bShrouded = fog.explored && !fog.explored->IsExplored(rTile);
        const bool bFogged = !bShrouded && fog.visible && !fog.visible->IsVisible(rTile);
        m_mapSurface.RenderTile(rGraphics, rTile, rShape, rWorldMap, bFogged, bShrouded, yieldOf,
                                MapGridStyle_t::WorldMapEdges, rDisplay.bOceanGrid, explored);
    });

    RenderBases_(rGraphics);
    RenderPathPreview_(rGraphics);
    m_unitMarkers.Render(rGraphics, m_rGameState, m_viewport);
}

} // namespace ac
