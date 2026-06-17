#include "ui/world/WorldView.h"
#include "ui/research/ResearchView.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/WorldMap.h"
#include "ui/UIManager.h"
#include "ui/TileHitTester.h"
#include "ui/UIPanel.h"
#include "graphics/Graphics.h"
#include <string>
#include <memory>

namespace ac
{

WorldView::WorldView(
    GameState& rGameState,
    Graphics& rGraphics,
    const WorldMap& rWorldMap,
    UIManager& rUIManager,
    std::function<void()> onProcessTurn,
    std::function<std::unique_ptr<IGameView>(BaseManager*)> onOpenBase
)
: m_rGameState(rGameState)
, m_pWorldDisplay(std::make_unique<WorldDisplay>(rGraphics))
, m_rUIManager(rUIManager)
, m_onProcessTurn(std::move(onProcessTurn))
, m_onOpenBase(std::move(onOpenBase))
{
    m_pWorldDisplay->SetWorldMap(&rWorldMap);

    m_pWorldMap = std::make_unique<WorldMapElement>();
    m_pWorldMap->SetPosition(0.f, 0.f);
    m_pWorldMap->SetSize(kWindowWidth, kWindowHeight - kInfoPanelHeight);
    m_pWorldMap->SetWorldDisplay(m_pWorldDisplay.get());

    m_pInfoPanel = std::make_unique<InfoPanelElement>();
}

void WorldView::Render(Graphics& rGraphics)
{
    m_pWorldMap->Draw(rGraphics);
    m_pInfoPanel->UpdateLayout(rGraphics);
    m_pInfoPanel->Draw(rGraphics);
    if (!m_lastClickedTileText.empty())
    {
        rGraphics.DrawText(m_lastClickedTileText, 20.f, 570.f, 18, Color::Yellow());
    }
}

void WorldView::Update(float deltaTime)
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
    m_pInfoPanel->SetInfoLines(infoLines);

    // Query current base info from GameState and update WorldDisplay
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

    m_pWorldMap->Update(deltaTime);
    m_pInfoPanel->Update(deltaTime);
}

void WorldView::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        m_rUIManager.RequestExit();
    }
    else if (rEvent.key == Key_t::Enter)
    {
        m_onProcessTurn();
    }
    else if (rEvent.key == Key_t::F2)
    {
        auto pResearchView = std::make_unique<ResearchView>(m_rUIManager);
        m_rUIManager.PushView(std::move(pResearchView));
    }
}

void WorldView::HandleMouse(const MouseEvent_t& rEvent)
{
    const WorldMap* pWorldMap = m_rGameState.GetWorldMap();
    if (!pWorldMap)
    {
        return;
    }
    const float tileSize = std::min(
        m_pWorldMap->GetWidth()  / static_cast<float>(pWorldMap->GetWidth()),
        m_pWorldMap->GetHeight() / static_cast<float>(pWorldMap->GetHeight()));

    auto tile = TileHitTester::HitTestWorldGrid(
        static_cast<float>(rEvent.x), static_cast<float>(rEvent.y),
        m_pWorldMap->GetX(), m_pWorldMap->GetY(), tileSize,
        pWorldMap->GetWidth(), pWorldMap->GetHeight());

    if (tile)
    {
        m_lastClickedTile = tile;
        m_lastClickedTileText = "Clicked tile: (" + std::to_string(tile->first) + ", " + std::to_string(tile->second) + ")";
        BaseManager* pBase = FindBaseAtTile_(tile->first, tile->second);
        if (pBase)
        {
            auto pBaseView = m_onOpenBase(pBase);
            if (pBaseView)
            {
                m_rUIManager.PushView(std::move(pBaseView));
            }
        }
    }
    else
    {
        m_lastClickedTile = std::nullopt;
        m_lastClickedTileText = "Clicked: (" + std::to_string(rEvent.x) + ", " + std::to_string(rEvent.y) + ") - no tile";
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
