#include "ui/world/WorldView.h"
#include <algorithm>
#include "ui/world/WorldMapElement.h"
#include "ui/world/InfoPanelElement.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/WorldMap.h"
#include "ui/TileHitTester.h"
#include "graphics/Graphics.h"
#include <string>
#include <memory>

namespace ac
{

WorldView::WorldView(
    GameState& rGameState,
    const WorldMap& rWorldMap,
    WindowLayout_t layout,
    std::function<void()> onProcessTurn,
    std::function<void()> onRequestExit,
    OpenBaseCallback_t onOpenBase
)
: IGameView(layout)
, m_rGameState(rGameState)
, m_pWorldDisplay(std::make_unique<WorldDisplay>())
, m_onProcessTurn(std::move(onProcessTurn))
, m_onRequestExit(std::move(onRequestExit))
, m_onOpenBase(std::move(onOpenBase))
{
    m_pWorldDisplay->SetWorldMap(&rWorldMap);

    auto pWorldMap = std::make_unique<WorldMapElement>(ResolveLayout(m_layout, k_MapLayout));
    pWorldMap->SetWorldDisplay(m_pWorldDisplay.get());
    m_elements.push_back(std::move(pWorldMap));

    m_elements.push_back(std::make_unique<InfoPanelElement>(ResolveLayout(m_layout, k_InfoPanelLayout)));
}

void WorldView::Render(Graphics& rGraphics)
{
    Update_();
    IGameView::Render(rGraphics);
}

void WorldView::Update_()
{
    std::vector<InfoPanelElement::InfoLine> infoLines;
    infoLines.push_back({"Mission Year: " + std::to_string(m_rGameState.GetMissionYear()), Color::White()});
    const auto& rFactions = m_rGameState.GetFactions();
    if (!rFactions.empty())
    {
        const Faction* pPlayerFaction = rFactions[0].get();
        infoLines.push_back({"Energy: " + std::to_string(pPlayerFaction->GetEnergy()), Color::Yellow()});
        infoLines.push_back({"Research: " + std::to_string(pPlayerFaction->GetResearchPoints()), Color{100, 200, 255, 255}});
    }
    static_cast<InfoPanelElement*>(m_elements[1].get())->SetInfoLines(infoLines);

    std::vector<BaseInfo_t> baseInfo;
    for (const auto& pFaction : m_rGameState.GetFactions())
    {
        for (const auto& pBase : pFaction->GetBases())
        {
            if (pBase)
            {
                // TODO: Track previousFactionId when base capture is implemented
                baseInfo.push_back({
                    pBase->GetX(),
                    pBase->GetY(),
                    pBase->GetName(),
                    pBase->GetFactionId(),
                    std::nullopt,  // previousFactionId - set when base is captured
                    pBase->GetBaseSize()
                });
            }
        }
    }
    m_pWorldDisplay->SetBaseInfo(baseInfo);
}

bool WorldView::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        m_onRequestExit();
        return true;
    }
    else if (rEvent.key == Key_t::Enter)
    {
        m_onProcessTurn();
        return true;
    }

    return HandleCameraKey_(rEvent);
}

bool WorldView::HandleCameraKey_(const KeyEvent_t& rEvent)
{
    const WorldMap* pWorldMap = m_rGameState.GetWorldMap();
    if (!pWorldMap)
    {
        return false;
    }

    const WindowLayout_t mapLayout = ResolveLayout(m_layout, k_MapLayout);
    const float tileSize = m_pWorldDisplay->GetEffectiveTileSize();
    const int visibleCols = static_cast<int>(mapLayout.width  / tileSize);
    const int visibleRows = static_cast<int>(mapLayout.height / tileSize);
    const int maxCamX = std::max(0, pWorldMap->GetWidth()  - visibleCols);
    const int maxCamY = std::max(0, pWorldMap->GetHeight() - visibleRows);

    int camX = m_pWorldDisplay->GetCameraX();
    int camY = m_pWorldDisplay->GetCameraY();

    if (rEvent.key == Key_t::ArrowLeft)
    {
        m_pWorldDisplay->SetCameraOffset(std::max(0, camX - 1), camY);
        return true;
    }
    else if (rEvent.key == Key_t::ArrowRight)
    {
        m_pWorldDisplay->SetCameraOffset(std::min(maxCamX, camX + 1), camY);
        return true;
    }
    else if (rEvent.key == Key_t::ArrowUp)
    {
        m_pWorldDisplay->SetCameraOffset(camX, std::max(0, camY - 1));
        return true;
    }
    else if (rEvent.key == Key_t::ArrowDown)
    {
        m_pWorldDisplay->SetCameraOffset(camX, std::min(maxCamY, camY + 1));
        return true;
    }

    return false;
}

void WorldView::HandleMouse(const MouseEvent_t& rEvent)
{
    const WorldMap* pWorldMap = m_rGameState.GetWorldMap();
    if (!pWorldMap)
    {
        return;
    }
    const WindowLayout_t mapLayout = ResolveLayout(m_layout, k_MapLayout);
    const float tileSize = m_pWorldDisplay->GetEffectiveTileSize();
    const int camX = m_pWorldDisplay->GetCameraX();
    const int camY = m_pWorldDisplay->GetCameraY();

    const int visibleCols = static_cast<int>(mapLayout.width  / tileSize);
    const int visibleRows = static_cast<int>(mapLayout.height / tileSize);

    auto tile = TileHitTester::HitTestWorldGrid(
        static_cast<float>(rEvent.x), static_cast<float>(rEvent.y),
        mapLayout.x, mapLayout.y, tileSize,
        visibleCols, visibleRows);

    if (tile)
    {
        const int worldX = tile->first  + camX;
        const int worldY = tile->second + camY;
        BaseManager* pBase = FindBaseAtTile_(worldX, worldY);
        if (pBase)
        {
            m_onOpenBase(*pBase);
        }
    }
}

BaseManager* WorldView::FindBaseAtTile_(int tileX, int tileY) const
{
    for (const auto& pFaction : m_rGameState.GetFactions())
    {
        for (const auto& pBase : pFaction->GetBases())
        {
            if (pBase && pBase->GetX() == tileX && pBase->GetY() == tileY)
            {
                return pBase.get();
            }
        }
    }
    return nullptr;
}

} // namespace ac
