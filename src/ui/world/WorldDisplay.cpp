#include "ui/world/WorldDisplay.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/FactionExploredMap.h"
#include "game/faction/FactionVisibleMap.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/RiverGeneration.h"
#include "game/map/ImprovementIds.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Pathfinder.h"
#include "game/units/Unit.h"
#include "ui/TileRenderer.h"
#include "ui/style/UiStyle.h"
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace ac
{

namespace
{

constexpr size_t k_BaseNameMinTruncChars        = 3;

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

WorldDisplay::WorldDisplay(const GameState& rGameState, WindowLayout_t layout)
    : m_rGameState(rGameState)
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

void WorldDisplay::SetCameraOffset(int tileX, int tileY)
{
    m_viewport.SetCamera(tileX, tileY);
}

float WorldDisplay::GetEffectiveTileSize() const
{
    return m_viewport.TileSize();
}

int WorldDisplay::GetCameraX() const
{
    return m_viewport.CameraX();
}

int WorldDisplay::GetCameraY() const
{
    return m_viewport.CameraY();
}

int WorldDisplay::GetVisibleCols() const
{
    return m_viewport.VisibleCols();
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

    for (const Faction& rFaction : m_rGameState.Factions())
    {
        for (const BaseManager& rBase : rFaction.Bases())
        {
            const Tile& rBaseTile = rBase.GetTile();
            const auto origin = m_viewport.PixelOriginOf(rBaseTile.GetX(), rBaseTile.GetY());
            if (!origin)
            {
                continue;
            }

            // Shroud hides bases entirely; fog still shows last-known bases.
            if (fog.explored && !fog.explored->IsExplored(rBaseTile))
            {
                continue;
            }

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

            const float textOffsetY = tileSize * s.baseTextOffsetRatio;

            // TODO: Use faction color for base marker based on rBase.GetFactionId()
            // TODO: Show capture animation when base capture is implemented
            // TODO: Show population size below name
            rGraphics.DrawText(displayName, origin->first + textOffsetX, origin->second + textOffsetY,
                               fontSize, s.baseNameColor);
        }
    }
}

void WorldDisplay::RenderSensors_(Graphics& rGraphics)
{
    const auto& s = Style().worldDisplay;
    const float tileSize = m_viewport.TileSize();
    const PlayerFogMaps_t fog = PlayerFog_(m_rGameState);

    const unsigned int fontSize = static_cast<unsigned int>(tileSize * s.sensorMarkerFontSizeRatio);
    const float markerWidth = tileSize * s.sensorMarkerWidthRatio;
    const float markerHeight = tileSize * s.sensorMarkerHeightRatio;
    const float inset = tileSize * s.sensorMarkerInsetRatio;

    m_viewport.ForEachVisibleTile([&](const Tile& rTile, float tileX, float tileY) {
        if (!rTile.HasImprovement(ImprovementIds::k_Sensor))
        {
            return;
        }

        // Shroud hides sensors; fog still shows last-known towers (same as bases).
        if (fog.explored && !fog.explored->IsExplored(rTile))
        {
            return;
        }

        // Top-right corner so the marker stays clear of base names and unit chips.
        const float markerX = tileX + tileSize - markerWidth - inset;
        const float markerY = tileY + inset;

        rGraphics.DrawFilledRect(markerX, markerY, markerWidth, markerHeight, s.sensorMarkerColor);
        rGraphics.DrawText("S", markerX + inset, markerY + inset, fontSize, s.sensorLabelColor);
    });
}

void WorldDisplay::RenderMonoliths_(Graphics& rGraphics)
{
    const auto& s = Style().worldDisplay;
    const float tileSize = m_viewport.TileSize();
    const PlayerFogMaps_t fog = PlayerFog_(m_rGameState);

    const unsigned int fontSize = static_cast<unsigned int>(tileSize * s.monolithMarkerFontSizeRatio);
    const float markerWidth = tileSize * s.monolithMarkerWidthRatio;
    const float markerHeight = tileSize * s.monolithMarkerHeightRatio;
    const float inset = tileSize * s.monolithMarkerInsetRatio;

    m_viewport.ForEachVisibleTile([&](const Tile& rTile, float tileX, float tileY) {
        if (!rTile.HasImprovement(ImprovementIds::k_Monolith))
        {
            return;
        }

        // Shroud hides monoliths; fog still shows last-known markers (same as bases/sensors).
        if (fog.explored && !fog.explored->IsExplored(rTile))
        {
            return;
        }

        // Centered so the marker reads as a tile landmark (Sensors own the top-right).
        const float markerX = tileX + (tileSize - markerWidth) * 0.5f;
        const float markerY = tileY + (tileSize - markerHeight) * 0.5f;

        rGraphics.DrawFilledRect(markerX, markerY, markerWidth, markerHeight, s.monolithMarkerColor);
        rGraphics.DrawText("M", markerX + inset, markerY + inset, fontSize, s.monolithLabelColor);
    });
}

void WorldDisplay::RenderRivers_(Graphics& rGraphics)
{
    const auto& s = Style().worldDisplay;
    const PlayerFogMaps_t fog = PlayerFog_(m_rGameState);
    const WorldMap& rWorldMap = m_viewport.GetWorldMap();
    const float thickness = std::max(1.0f, m_viewport.TileSize() * s.riverLineThicknessRatio);
    const float stub = m_viewport.TileSize() * 0.2f;

    m_viewport.ForEachVisibleTile([&](const Tile& rTile, float /*tileX*/, float /*tileY*/) {
        // Rivers stay under shroud; only explored river tiles seed drawing.
        if (fog.explored && !fog.explored->IsExplored(rTile))
        {
            return;
        }
        if (!rTile.GetHasRiver())
        {
            return;
        }

        const auto from = m_viewport.PixelCenterOf(rTile);
        if (!from)
        {
            return;
        }

        const RiverConnection_t connections = GetRiverConnections(rTile, rWorldMap);

        // Isolated river tile (source/sink with no river neighbor): short cross so it shows.
        if (connections == RiverConnection_t::None)
        {
            rGraphics.DrawLine(from->first - stub, from->second, from->first + stub, from->second,
                               s.riverColor, thickness);
            rGraphics.DrawLine(from->first, from->second - stub, from->first, from->second + stub,
                               s.riverColor, thickness);
            return;
        }

        // East/South edges avoid double-drawing when both tiles are explored. North/West
        // edges are drawn only into shrouded neighbors so a river can flow "off the map
        // of knowledge" without waiting for the far tile to be explored.
        static constexpr RiverConnection_t k_DrawDirs[4] = {
            RiverConnection_t::North,
            RiverConnection_t::East,
            RiverConnection_t::South,
            RiverConnection_t::West,
        };
        static constexpr int k_Deltas[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};

        for (int i = 0; i < 4; ++i)
        {
            if (!HasRiverConnection(connections, k_DrawDirs[i]))
            {
                continue;
            }
            const Tile* pNeighbor =
                rWorldMap.GetTile(rTile.GetX() + k_Deltas[i][0], rTile.GetY() + k_Deltas[i][1]);
            if (!pNeighbor)
            {
                continue;
            }
            const bool bNeighborExplored =
                !fog.explored || fog.explored->IsExplored(*pNeighbor);
            const bool bEastOrSouth =
                k_DrawDirs[i] == RiverConnection_t::East
                || k_DrawDirs[i] == RiverConnection_t::South;
            if (!bEastOrSouth && bNeighborExplored)
            {
                continue;
            }
            const auto to = m_viewport.PixelCenterOf(*pNeighbor);
            if (!to)
            {
                continue;
            }
            rGraphics.DrawLine(from->first, from->second, to->first, to->second,
                               s.riverColor, thickness);
        }
    });
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
        rGraphics.DrawLine(centers[i - 1].first, centers[i - 1].second,
                           centers[i].first, centers[i].second,
                           s.pathPreviewColor, thickness);
    }
}

void WorldDisplay::Render(Graphics& rGraphics)
{
    const WorldMap& rWorldMap = m_viewport.GetWorldMap();
    if (rWorldMap.GetWidth() <= 0 || rWorldMap.GetHeight() <= 0)
    {
        return;
    }

    const PlayerFogMaps_t fog = PlayerFog_(m_rGameState);
    const float tileSize = m_viewport.TileSize();

    m_viewport.ForEachVisibleTile([&](const Tile& rTile, float tileX, float tileY) {
        if (fog.explored && !fog.explored->IsExplored(rTile))
        {
            rGraphics.DrawFilledRect(tileX, tileY, tileSize, tileSize,
                                     Style().worldDisplay.shroudColor);
            return;
        }

        const bool bFogged = fog.visible && !fog.visible->IsVisible(rTile);
        TileRenderer::Render(rGraphics, rTile, tileX, tileY, tileSize, bFogged);
    });

    RenderBases_(rGraphics);
    RenderRivers_(rGraphics);
    RenderPathPreview_(rGraphics);
    RenderSensors_(rGraphics);
    RenderMonoliths_(rGraphics);
    m_unitMarkers.Render(rGraphics, m_rGameState, m_viewport);
}

} // namespace ac
