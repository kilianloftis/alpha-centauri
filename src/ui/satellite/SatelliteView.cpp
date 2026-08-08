#include "ui/satellite/SatelliteView.h"

#include "ui/NoticePopup.h"
#include "ui/satellite/OrbitalAttackerPopup.h"
#include "ui/satellite/SatelliteButtonListPanel.h"
#include "ui/satellite/SatelliteLabeledButton.h"
#include "ui/satellite/SatelliteSummaryPanel.h"
#include "game/Faction.h"
#include "game/GameState.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/orbital/OrbitalAttack.h"
#include "game/orbital/OrbitalCensus.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace ac
{

namespace
{

std::string BuildingDisplayName_(const BuildingRegistry& rBuildings, const BuildingId_t& id)
{
    if (const BuildingConfig_t* pConfig = rBuildings.Find(id))
    {
        return pConfig->name;
    }
    return id;
}

std::string FormatAttackOutcome_(const OrbitalAttackResult_t& rResult,
                                 const BuildingRegistry& rBuildings)
{
    if (!rResult.bAttempted)
    {
        return "The attack could not be completed.";
    }

    const std::string targetName =
        BuildingDisplayName_(rBuildings, rResult.targetBuildingId);
    if (rResult.bHit)
    {
        return "Attack successful. Destroyed one " + targetName + ".";
    }

    std::string message = "Attack failed against " + targetName + ".";
    if (rResult.bAttackerDestroyed)
    {
        message += " Our "
            + BuildingDisplayName_(rBuildings, rResult.attackerBuildingId)
            + " was destroyed.";
    }
    return message;
}

} // namespace

SatelliteView::SatelliteView(GameState& rGameState,
                             const BuildingRegistry& rBuildings,
                             WindowLayout_t layout)
    : IGameView(layout)
    , m_rGameState(rGameState)
    , m_rBuildings(rBuildings)
{
    Rebuild_();
}

void SatelliteView::Render(Graphics& rGraphics)
{
    if (m_bPendingAttackRefresh)
    {
        m_bPendingAttackRefresh = false;
        std::optional<std::string> outcome = std::move(m_pendingOutcomeMessage);
        m_pendingOutcomeMessage.reset();
        // Only the target list needs it: an attack is reachable only in OrbitalAttack mode,
        // and switching back to Summary rebuilds that panel, whose constructor re-censuses.
        RefreshTargetList_();
        if (outcome)
        {
            ShowOutcome_(std::move(*outcome));
        }
    }

    const auto& style = Style().satelliteView;
    const WindowLayout_t topPanel = ResolveLayout(m_layout, Style().layouts.topPanel);
    rGraphics.DrawFilledRect(
        topPanel.x, topPanel.y, topPanel.width, topPanel.height, style.backgroundColor);
    rGraphics.DrawRect(
        topPanel.x, topPanel.y, topPanel.width, topPanel.height, style.borderColor);

    IGameView::Render(rGraphics);
}

bool SatelliteView::HandleKey(const KeyEvent_t& rEvent)
{
    if (IGameView::HandleKey(rEvent))
    {
        return true;
    }
    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
        return true;
    }
    return false;
}

void SatelliteView::SetMode_(Mode_t mode)
{
    if (m_mode == mode)
    {
        return;
    }
    m_mode = mode;
    Rebuild_();
}

void SatelliteView::SelectFaction_(FactionId_t factionId)
{
    if (m_selectedFactionId && *m_selectedFactionId == factionId)
    {
        return;
    }
    m_selectedFactionId = factionId;
    m_selectedBuildingId.reset();

    // In place: a full rebuild would destroy the button whose callback is running.
    if (m_pFactionList)
    {
        m_pFactionList->SetSelected(std::to_string(factionId));
    }
    RefreshTargetList_();
}

void SatelliteView::SelectTarget_(BuildingId_t buildingId)
{
    if (m_selectedBuildingId && *m_selectedBuildingId == buildingId)
    {
        return;
    }
    m_selectedBuildingId = std::move(buildingId);
    if (m_pTargetList)
    {
        m_pTargetList->SetSelected(m_selectedBuildingId);
    }
}

std::vector<std::string> SatelliteView::CollectMissingAttackPrerequisites_() const
{
    std::vector<std::string> missing;
    if (!m_selectedFactionId)
    {
        missing.push_back("a target faction");
    }
    if (!m_selectedBuildingId)
    {
        missing.push_back("a target satellite");
    }
    return missing;
}

void SatelliteView::OnAttackClicked_()
{
    // The Attack control is always enabled, so a dead click was the only feedback for
    // "you have not picked a target yet".
    const std::vector<std::string> missing = CollectMissingAttackPrerequisites_();
    if (!missing.empty())
    {
        std::string message = "Select ";
        for (size_t i = 0; i < missing.size(); ++i)
        {
            if (i > 0)
            {
                message += " and ";
            }
            message += missing[i];
        }
        ShowOutcome_(message + " before attacking.");
        return;
    }
    OpenAttackerPopup_(*m_selectedFactionId, *m_selectedBuildingId);
}

void SatelliteView::ShowOutcome_(std::string message)
{
    DismissOpenModals_();
    m_elements.push_back(std::make_unique<NoticePopup>(
        ResolveLayout(m_layout, Style().layouts.popupSmall),
        "Orbital Attack",
        std::move(message)));
}

void SatelliteView::OpenAttackerPopup_(FactionId_t targetFactionId,
                                       BuildingId_t targetBuildingId)
{
    // A missing player faction during a player-driven view is a broken session, not an empty
    // state — contrast the explicit outcome string used when there are simply no attackers.
    Faction* pPlayer = m_rGameState.GetPlayerFaction();
    if (!pPlayer)
    {
        throw std::runtime_error("SatelliteView: no player faction; cannot open the attacker list");
    }

    auto attackers = m_rGameState.ListReadyOrbitalAttackers(*pPlayer);
    if (attackers.empty())
    {
        ShowOutcome_("No ready orbital attackers.");
        return;
    }

    DismissOpenModals_();
    m_elements.push_back(std::make_unique<OrbitalAttackerPopup>(
        ResolveLayout(m_layout, Style().layouts.popupSmall),
        std::move(attackers),
        [this, targetFactionId, targetBuildingId](BuildingId_t attackerId) {
            CommenceAttack_(std::move(attackerId), targetFactionId, targetBuildingId);
        }));
}

void SatelliteView::CommenceAttack_(BuildingId_t attackerBuildingId,
                                    FactionId_t targetFactionId,
                                    BuildingId_t targetBuildingId)
{
    Faction* pPlayer = m_rGameState.GetPlayerFaction();
    Faction* pTarget = m_rGameState.FindFaction(targetFactionId);
    if (!pPlayer || !pTarget)
    {
        ShowOutcome_("The attack could not be completed.");
        return;
    }

    const OrbitalAttackResult_t result = m_rGameState.TryAttackSatellite(
        *pPlayer, *pTarget, attackerBuildingId, targetBuildingId);
    m_pendingOutcomeMessage = FormatAttackOutcome_(result, m_rBuildings);
    m_bPendingAttackRefresh = true;
}

void SatelliteView::Rebuild_()
{
    m_elements.clear();
    m_pFactionList = nullptr;
    m_pTargetList = nullptr;

    const auto& style = Style().satelliteView;
    const WindowLayout_t topPanel = ResolveLayout(m_layout, Style().layouts.topPanel);

    m_elements.push_back(std::make_unique<SatelliteLabeledButton>(
        ResolveLayout(topPanel, style.summaryTabLayout),
        "Satellite Summary",
        [this]() { SetMode_(Mode_t::Summary); },
        m_mode == Mode_t::Summary));

    m_elements.push_back(std::make_unique<SatelliteLabeledButton>(
        ResolveLayout(topPanel, style.attackTabLayout),
        "Orbital Attack View",
        [this]() { SetMode_(Mode_t::OrbitalAttack); },
        m_mode == Mode_t::OrbitalAttack));

    if (m_mode == Mode_t::Summary)
    {
        // Its constructor takes the census; nothing can move the census in this mode.
        m_elements.push_back(std::make_unique<SatelliteSummaryPanel>(
            m_rGameState,
            m_rBuildings,
            ResolveLayout(topPanel, style.contentLayout)));
        return;
    }

    // Attack mode: topPanel holds only the view tabs and Attack; lists use dashboard panels.
    m_elements.push_back(std::make_unique<SatelliteLabeledButton>(
        ResolveLayout(topPanel, style.attackButtonLayout),
        "Attack",
        [this]() { OnAttackClicked_(); },
        /*bSelected*/ false));

    std::vector<SatelliteButtonListPanel::Item_t> factionItems;
    const Faction* pPlayer = m_rGameState.GetPlayerFaction();
    for (const Faction& rFaction : m_rGameState.Factions())
    {
        if (pPlayer && rFaction.GetFactionId() == pPlayer->GetFactionId())
        {
            continue;
        }
        factionItems.push_back(SatelliteButtonListPanel::Item_t{
            std::to_string(rFaction.GetFactionId()),
            rFaction.GetDefinition().identity.name});
    }

    std::optional<std::string> selectedFactionKey;
    if (m_selectedFactionId)
    {
        selectedFactionKey = std::to_string(*m_selectedFactionId);
    }

    auto pFactionList = std::make_unique<SatelliteButtonListPanel>(
        ResolveLayout(m_layout, Style().layouts.centerPanel),
        "Enemy Factions",
        std::move(factionItems),
        selectedFactionKey,
        [this](const std::string& rId) { SelectFaction_(std::stoi(rId)); });
    m_pFactionList = pFactionList.get();
    m_elements.push_back(std::move(pFactionList));

    auto pTargetList = std::make_unique<SatelliteButtonListPanel>(
        ResolveLayout(m_layout, Style().layouts.rightPanel),
        "Orbital Targets",
        std::vector<SatelliteButtonListPanel::Item_t>{},
        m_selectedBuildingId,
        [this](const std::string& rId) { SelectTarget_(rId); });
    m_pTargetList = pTargetList.get();
    m_elements.push_back(std::move(pTargetList));

    RefreshTargetList_();
}

void SatelliteView::RefreshTargetList_()
{
    if (!m_pTargetList)
    {
        return;
    }

    std::unordered_map<BuildingId_t, int> ownedCounts;
    if (m_selectedFactionId)
    {
        for (const OrbitalCensusEntry_t& rEntry : m_rGameState.GetOrbitalCensus())
        {
            if (rEntry.factionId == *m_selectedFactionId && rEntry.count > 0)
            {
                ownedCounts[rEntry.buildingId] = rEntry.count;
            }
        }
    }

    if (m_selectedBuildingId && ownedCounts.find(*m_selectedBuildingId) == ownedCounts.end())
    {
        m_selectedBuildingId.reset();
    }

    std::vector<SatelliteButtonListPanel::Item_t> targetItems;
    for (const BuildingConfig_t& rBuilding : m_rBuildings.GetAll())
    {
        if (!rBuilding.orbital)
        {
            continue;
        }
        const auto it = ownedCounts.find(rBuilding.id);
        if (it == ownedCounts.end())
        {
            continue;
        }
        targetItems.push_back(SatelliteButtonListPanel::Item_t{
            rBuilding.id,
            rBuilding.name + " (" + std::to_string(it->second) + ")"});
    }

    m_pTargetList->SetItems(std::move(targetItems), m_selectedBuildingId);
}

} // namespace ac
