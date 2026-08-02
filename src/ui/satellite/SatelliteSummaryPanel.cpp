#include "ui/satellite/SatelliteSummaryPanel.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/orbital/OrbitalCensus.h"
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
}

void SatelliteSummaryPanel::Render(Graphics& rGraphics)
{
    const auto& style = Style().satelliteView;
    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.backgroundColor);
    rGraphics.DrawRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.borderColor);

    std::vector<const BuildingConfig_t*> orbitalTypes;
    for (const BuildingConfig_t& rBuilding : m_rBuildings.GetAll())
    {
        if (rBuilding.orbital)
        {
            orbitalTypes.push_back(&rBuilding);
        }
    }

    std::vector<const Faction*> factions;
    for (const Faction& rFaction : m_rGameState.Factions())
    {
        factions.push_back(&rFaction);
    }

    if (orbitalTypes.empty() || factions.empty())
    {
        rGraphics.DrawText(
            "No orbital data",
            m_layout.x + style.paddingRatio * m_layout.width,
            m_layout.y + style.paddingRatio * m_layout.height,
            style.headerFontSize,
            style.headerColor);
        return;
    }

    std::unordered_map<std::string, int> counts;
    for (const OrbitalCensusEntry_t& rEntry : m_rGameState.GetOrbitalCensus())
    {
        counts[std::to_string(rEntry.factionId) + ":" + rEntry.buildingId] = rEntry.count;
    }

    // +1 column for faction names; +1 row for building headers.
    const int cols = static_cast<int>(orbitalTypes.size()) + 1;
    const int rows = static_cast<int>(factions.size()) + 1;
    const float cellW = m_layout.width / static_cast<float>(cols);
    const float cellH = m_layout.height / static_cast<float>(rows);
    const float padX = style.paddingRatio * cellW;
    const float padY = style.paddingRatio * cellH;

    for (int col = 0; col < cols; ++col)
    {
        const float x = m_layout.x + static_cast<float>(col) * cellW;
        rGraphics.DrawRect(x, m_layout.y, cellW, m_layout.height, style.borderColor);
        if (col == 0)
        {
            continue;
        }
        const BuildingConfig_t* pBuilding = orbitalTypes[static_cast<size_t>(col - 1)];
        rGraphics.DrawText(
            pBuilding->name,
            x + padX,
            m_layout.y + padY,
            style.headerFontSize,
            style.headerColor);
    }

    for (int row = 0; row < rows; ++row)
    {
        const float y = m_layout.y + static_cast<float>(row) * cellH;
        rGraphics.DrawRect(m_layout.x, y, m_layout.width, cellH, style.borderColor);
        if (row == 0)
        {
            continue;
        }
        const Faction* pFaction = factions[static_cast<size_t>(row - 1)];
        rGraphics.DrawText(
            pFaction->GetDefinition().identity.name,
            m_layout.x + padX,
            y + padY,
            style.factionFontSize,
            style.factionNameColor);

        for (size_t col = 0; col < orbitalTypes.size(); ++col)
        {
            const BuildingId_t& buildingId = orbitalTypes[col]->id;
            const std::string key =
                std::to_string(pFaction->GetFactionId()) + ":" + buildingId;
            const auto it = counts.find(key);
            const int count = it != counts.end() ? it->second : 0;
            const float cellX = m_layout.x + static_cast<float>(col + 1) * cellW;
            rGraphics.DrawText(
                std::to_string(count),
                cellX + padX,
                y + padY,
                style.cellFontSize,
                style.cellColor);
        }
    }
}

} // namespace ac
