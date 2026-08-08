#pragma once

#include "game/buildings/BuildingConfig.h"
#include "game/faction/base/BaseTypes.h"
#include "ui/IGameView.h"

#include <optional>
#include <string>
#include <vector>

namespace ac
{

class GameState;
class BuildingRegistry;
class Graphics;
class SatelliteButtonListPanel;
class SatelliteSummaryPanel;

class SatelliteView : public IGameView
{
public:
    SatelliteView(GameState& rGameState,
                  const BuildingRegistry& rBuildings,
                  WindowLayout_t layout);

    void Render(Graphics& rGraphics) override;
    bool HandleKey(const KeyEvent_t& rEvent) override;

private:
    enum class Mode_t
    {
        Summary,
        OrbitalAttack,
    };

    void Rebuild_();
    // Target list contents follow the selected faction; the faction list itself does not move.
    void RefreshTargetList_();
    std::vector<std::string> CollectMissingAttackPrerequisites_() const;
    void SetMode_(Mode_t mode);
    void SelectFaction_(FactionId_t factionId);
    void SelectTarget_(BuildingId_t buildingId);
    void OnAttackClicked_();
    void OpenAttackerPopup_(FactionId_t targetFactionId, BuildingId_t targetBuildingId);
    void CommenceAttack_(BuildingId_t attackerBuildingId,
                         FactionId_t targetFactionId,
                         BuildingId_t targetBuildingId);
    void ShowOutcome_(std::string message);

    GameState& m_rGameState;
    const BuildingRegistry& m_rBuildings;
    Mode_t m_mode = Mode_t::Summary;
    // Owned by m_elements; cleared and re-pointed by Rebuild_, which is the only thing that
    // replaces them. Null in the mode that does not build them.
    SatelliteSummaryPanel* m_pSummaryPanel = nullptr;
    SatelliteButtonListPanel* m_pFactionList = nullptr;
    SatelliteButtonListPanel* m_pTargetList = nullptr;
    std::optional<FactionId_t> m_selectedFactionId;
    std::optional<BuildingId_t> m_selectedBuildingId;
    // Deferred so attacker-popup callbacks do not destroy the popup mid-stack.
    bool m_bPendingAttackRefresh = false;
    std::optional<std::string> m_pendingOutcomeMessage;
};

} // namespace ac
