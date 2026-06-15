#include "ui/world/WorldView.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "ui/UIManager.h"
#include "ui/TileHitTester.h"
#include "ui/world/WorldDisplay.h"
#include "graphics/Graphics.h"
#include <string>

namespace ac
{

WorldView::WorldView(
    GameState& rGameState,
    WorldDisplay& rWorldDisplay,
    UIManager& rUIManager,
    std::function<void()> onProcessTurn,
    std::function<std::unique_ptr<IGameView>(BaseManager*)> onOpenBase
)
: m_rGameState(rGameState)
, m_rWorldDisplay(rWorldDisplay)
, m_rUIManager(rUIManager)
, m_onProcessTurn(std::move(onProcessTurn))
, m_onOpenBase(std::move(onOpenBase))
{
    m_pWorldMap = std::make_unique<WorldMapElement>();
    m_pWorldMap->SetPosition(0.f, 0.f);
    m_pWorldMap->SetSize(kWindowWidth, kWindowHeight - kInfoPanelHeight);
    m_pWorldMap->SetWorldDisplay(&rWorldDisplay);

    m_pInfoPanel = std::make_unique<InfoPanelElement>();
    m_pInfoPanel->SetPosition(0.f, kWindowHeight - kInfoPanelHeight);
    m_pInfoPanel->SetSize(kWindowWidth, kInfoPanelHeight);
}

void WorldView::Render(Graphics& rGraphics)
{
    m_pWorldMap->Draw(rGraphics);
    m_pInfoPanel->Draw(rGraphics);
    if (!m_lastClickedTileText.empty())
    {
        rGraphics.DrawText(m_lastClickedTileText, 20.f, 570.f, 18, Color::Yellow());
    }
}

void WorldView::Update(float deltaTime)
{
    std::vector<UIPanel::InfoLine> infoLines;
    infoLines.push_back({"Mission Year: " + std::to_string(m_rGameState.GetMissionYear()), Color::White()});
    const auto& rFactions = m_rGameState.GetFactions();
    if (!rFactions.empty())
    {
        const Faction* pPlayerFaction = rFactions[0].get();
        infoLines.push_back({"Energy: " + std::to_string(pPlayerFaction->GetEnergy()), Color::Yellow()});
        infoLines.push_back({"Research: " + std::to_string(pPlayerFaction->GetResearchPoints()), Color{100, 200, 255, 255}});
    }
    m_pInfoPanel->SetInfoLines(infoLines);
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
        for (size_t i = 0; i < pFaction->GetBaseCount(); ++i)
        {
            BaseManager* pBase = pFaction->GetBase(i);
            if (pBase && pBase->GetX() == tileX && pBase->GetY() == tileY)
            {
                return pBase;
            }
        }
    }
    return nullptr;
}

} // namespace ac
