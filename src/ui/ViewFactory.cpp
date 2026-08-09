#include "ui/ViewFactory.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/Military.h"
#include "game/faction/ResearchManager.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitDesignAvailability.h"
#include "game/units/UnitSlotRegistry.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

#include <stdexcept>

namespace ac
{

Faction* ViewFactory::RequirePlayerFaction_() const
{
    Faction* pFaction = m_rGameState.GetPlayerFaction();
    if (!pFaction)
    {
        throw std::runtime_error("ViewFactory: no player faction; cannot build a player view");
    }
    return pFaction;
}

ViewFactory::ViewFactory(
    GameState& rGameState,
    GameDataContext& rGameDataContext,
    Graphics& rGraphics,
    GameSettings& rSettings
)
    : m_rGameState(rGameState)
    , m_rGameDataContext(rGameDataContext)
    , m_rGraphics(rGraphics)
    , m_rSettings(rSettings)
{
}

std::unique_ptr<WorldView> ViewFactory::CreateWorldView(
    const WindowLayout_t& layout,
    std::function<void()> onProcessTurn,
    std::function<void()> onRequestExit,
    std::function<void(BaseManager&)> onOpenBase,
    WorldView::OpenCombatCallback_t onOpenCombat,
    std::function<void()> onOpenCommlinks
) const
{
    return std::make_unique<WorldView>(
        m_rGameState,
        m_rGameDataContext,
        m_rGameState.GetWorldMap(),
        layout,
        std::move(onProcessTurn),
        std::move(onRequestExit),
        std::move(onOpenBase),
        std::move(onOpenCombat),
        std::move(onOpenCommlinks));
}

std::unique_ptr<BaseView> ViewFactory::CreateBaseView(BaseManager& rBase) const
{
    return CreateBaseView(rBase, GetFullscreenLayout());
}

std::unique_ptr<BaseView> ViewFactory::CreateBaseView(
    BaseManager& rBase,
    const WindowLayout_t& layout
) const
{
    const Faction* pFaction = RequirePlayerFaction_();

    const bool bEditable = (rBase.GetFactionId() == pFaction->GetFactionId());
    return std::make_unique<BaseView>(rBase, layout, bEditable);
}

std::unique_ptr<ResearchView> ViewFactory::CreateResearchView(
    const WindowLayout_t& layout
) const
{
    const Faction* pFaction = RequirePlayerFaction_();

    return std::make_unique<ResearchView>(pFaction->GetResearch(), layout);
}

std::unique_ptr<SocialEngineeringView> ViewFactory::CreateSocialEngineeringView(
    const WindowLayout_t& layout
) const
{
    Faction* pFaction = RequirePlayerFaction_();

    return std::make_unique<SocialEngineeringView>(
        *pFaction,
        *m_rGameDataContext.socialPolicyRegistry,
        *m_rGameDataContext.socialRatingRegistry,
        layout
    );
}

std::unique_ptr<UnitDesignerView> ViewFactory::CreateUnitDesignerView(
    const WindowLayout_t& layout
) const
{
    Faction* pFaction = RequirePlayerFaction_();
    const std::vector<std::string>& rDiscoveredTechs =
        pFaction->GetResearch().GetDiscoveredTechs();

    return std::make_unique<UnitDesignerView>(
        pFaction->GetMilitary(),
        GetAvailableUnitSlots(*m_rGameDataContext.unitSlotRegistry, rDiscoveredTechs),
        GetAvailableUnitComponents(*m_rGameDataContext.unitComponentRegistry, rDiscoveredTechs),
        &pFaction->GetUnitManager(),
        layout
    );
}

std::unique_ptr<SettingsView> ViewFactory::CreateSettingsView(
    const WindowLayout_t& layout
) const
{
    return std::make_unique<SettingsView>(m_rSettings, layout);
}

std::unique_ptr<CommlinksView> ViewFactory::CreateCommlinksView(
    const WindowLayout_t& layout,
    std::function<void()> onOpenCouncilVote
) const
{
    return std::make_unique<CommlinksView>(
        m_rGameState, layout, std::move(onOpenCouncilVote));
}

std::unique_ptr<CouncilVoteView> ViewFactory::CreateCouncilVoteView(
    const WindowLayout_t& layout
) const
{
    return std::make_unique<CouncilVoteView>(m_rGameState, layout);
}

std::unique_ptr<SatelliteView> ViewFactory::CreateSatelliteView(
    const WindowLayout_t& layout
) const
{
    return std::make_unique<SatelliteView>(
        m_rGameState, *m_rGameDataContext.buildingRegistry, layout);
}

std::unique_ptr<CombatView> ViewFactory::CreateCombatView(
    const WindowLayout_t& layout,
    CombatResult_t result,
    const Tile& rAttackerTile,
    const Tile& rDefenderTile,
    std::string attackerName,
    std::string defenderName,
    WorldDisplay& rWorldDisplay,
    WindowLayout_t mapLayout,
    std::function<void()> onFinished
) const
{
    return std::make_unique<CombatView>(
        layout,
        std::move(result),
        rAttackerTile,
        rDefenderTile,
        std::move(attackerName),
        std::move(defenderName),
        rWorldDisplay,
        mapLayout,
        std::move(onFinished));
}

WindowLayout_t ViewFactory::GetFullscreenLayout() const
{
    const auto& s = Style().viewFactory;
    return {
        s.fullscreenOriginX,
        s.fullscreenOriginY,
        static_cast<float>(m_rGraphics.GetWindowWidth()),
        static_cast<float>(m_rGraphics.GetWindowHeight())
    };
}

} // namespace ac
