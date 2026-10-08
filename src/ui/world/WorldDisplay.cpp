#include "ui/world/WorldDisplay.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/faction/UnitVisibility.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Pathfinder.h"
#include "game/units/Unit.h"
#include "ui/style/UiStyle.h"
#include "ui/world/MapAppearance.h"
#include <algorithm>
#include <utility>
#include <vector>

namespace ac
{

WorldDisplay::WorldDisplay(const GameState& rGameState, MapRenderer& rMapRenderer,
                           WindowLayout_t layout)
    : m_rGameState(rGameState)
    , m_rMapRenderer(rMapRenderer)
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
}

void WorldDisplay::SetPlaybackVisibleUnits(const std::unordered_set<UnitId_t>* pUnitIds)
{
    m_pPlaybackVisibleUnits = pUnitIds;
}

float WorldDisplay::GetEffectiveTileSize() const
{
    return m_viewport.TileSize();
}

int WorldDisplay::GetVisibleRows() const
{
    return m_viewport.VisibleRows();
}

std::optional<Rectangle_t> WorldDisplay::MarkerRectOf(UnitId_t unitId) const
{
    const auto it = m_unitMarkers.find(unitId);
    if (it == m_unitMarkers.end())
    {
        return std::nullopt;
    }
    return it->second;
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

    m_viewport.SetRelief(m_rGameState.GetSettings().GetMapDisplay().relief,
                         Style().tileRenderer.relief);
    const Faction* pPlayer = m_rGameState.GetPlayerFaction();

    MapContent_t content;
    content.showsBase = [](const BaseManager&) { return true; };
    content.showsUnit = [this, pPlayer](const Unit& rUnit) {
        // Per-unit visibility (fog, Conceal/Detect, contact reveal), not tile fog alone.
        return (m_pPlaybackVisibleUnits && m_pPlaybackVisibleUnits->contains(rUnit.GetUnitId()))
               || !pPlayer || IsUnitVisibleTo(*pPlayer, rUnit, m_rGameState.GetTileEffects());
    };
    content.pSelectedUnit = m_pSelectedUnit;

    m_unitMarkers = m_rMapRenderer.Render(rGraphics, m_viewport.VisibleTiles(),
                                          MapAppearance::Fogged(rWorldMap, pPlayer), content);
    RenderPathPreview_(rGraphics);
}

} // namespace ac
