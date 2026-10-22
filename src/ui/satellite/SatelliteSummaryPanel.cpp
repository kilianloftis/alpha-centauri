#include "ui/satellite/SatelliteSummaryPanel.h"

#include "game/faction/Faction.h"
#include "game/GameState.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/buildings/OrbitalCensus.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace ac
{

SatelliteSummaryPanel::SatelliteSummaryPanel(GameState& rGameState,
                                             const BuildingRegistry& rBuildings,
                                             WindowLayout_t layout)
    : UIElement(layout)
    , m_rGameState(rGameState)
    , m_rBuildings(rBuildings)
{
    Refresh();
}

void SatelliteSummaryPanel::Refresh()
{
    m_orbitalTypes.clear();
    for (const BuildingConfig_t& rBuilding : m_rBuildings.GetAll())
    {
        if (rBuilding.orbital)
        {
            m_orbitalTypes.push_back(&rBuilding);
        }
    }

    m_factions.clear();
    for (const Faction& rFaction : m_rGameState.Factions())
    {
        m_factions.push_back(&rFaction);
    }

    m_counts.clear();
    for (const OrbitalCensusEntry_t& rEntry : BuildOrbitalCensus())
    {
        m_counts[std::to_string(rEntry.factionId) + ":" + rEntry.buildingId] = rEntry.count;
    }
}

void SatelliteSummaryPanel::Render(Graphics& rGraphics)
{
    const auto& style = Style().satelliteView;
    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.backgroundColor);
    rGraphics.DrawRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.borderColor);

    if (m_orbitalTypes.empty() || m_factions.empty())
    {
        rGraphics.DrawText(
            "No orbital data",
            m_layout.x + style.paddingRatio * m_layout.width,
            m_layout.y + style.paddingRatio * m_layout.height,
            style.headerFontSize,
            style.headerColor);
        return;
    }

    // +1 column for faction names; +1 row for building headers.
    const int cols = static_cast<int>(m_orbitalTypes.size()) + 1;
    const int rows = static_cast<int>(m_factions.size()) + 1;
    const float cellW = m_layout.width / static_cast<float>(cols);
    const float cellH = m_layout.height / static_cast<float>(rows);
    const Grid_t grid{cols, rows, cellW, cellH, style.paddingRatio * cellW,
                      style.paddingRatio * cellH};
    DrawBuildingColumns_(rGraphics, grid);
    DrawFactionRows_(rGraphics, grid);
}

void SatelliteSummaryPanel::DrawBuildingColumns_(Graphics& rGraphics, const Grid_t& rGrid) const
{
    const auto& style = Style().satelliteView;
    for (int col = 0; col < rGrid.cols; ++col)
    {
        const float x = m_layout.x + static_cast<float>(col) * rGrid.cellW;
        rGraphics.DrawRect(x, m_layout.y, rGrid.cellW, m_layout.height, style.borderColor);
        if (col == 0)
        {
            continue;
        }
        const BuildingConfig_t* pBuilding = m_orbitalTypes[static_cast<size_t>(col - 1)];
        rGraphics.DrawText(pBuilding->name, x + rGrid.padX, m_layout.y + rGrid.padY,
                           style.headerFontSize, style.headerColor);
    }
}

void SatelliteSummaryPanel::DrawFactionRows_(Graphics& rGraphics, const Grid_t& rGrid) const
{
    const auto& style = Style().satelliteView;
    for (int row = 0; row < rGrid.rows; ++row)
    {
        const float y = m_layout.y + static_cast<float>(row) * rGrid.cellH;
        rGraphics.DrawRect(m_layout.x, y, m_layout.width, rGrid.cellH, style.borderColor);
        if (row == 0)
        {
            continue;
        }
        const Faction* pFaction = m_factions[static_cast<size_t>(row - 1)];
        rGraphics.DrawText(pFaction->GetDefinition().identity.name, m_layout.x + rGrid.padX,
                           y + rGrid.padY, style.factionFontSize, style.factionNameColor);

        for (size_t col = 0; col < m_orbitalTypes.size(); ++col)
        {
            const BuildingId_t& buildingId = m_orbitalTypes[col]->id;
            const std::string key =
                std::to_string(pFaction->GetFactionId()) + ":" + buildingId;
            const auto it = m_counts.find(key);
            const int count = it != m_counts.end() ? it->second : 0;
            const float cellX = m_layout.x + static_cast<float>(col + 1) * rGrid.cellW;
            rGraphics.DrawText(std::to_string(count), cellX + rGrid.padX, y + rGrid.padY,
                               style.cellFontSize, style.cellColor);
        }
    }
}

} // namespace ac
