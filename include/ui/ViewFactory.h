#pragma once

#include "ui/UIElement.h"
#include "ui/IGameView.h"
#include "ui/base/BaseView.h"
#include "ui/research/ResearchView.h"
#include "ui/commlinks/CommlinksView.h"
#include "ui/council/CouncilVoteView.h"
#include "ui/satellite/SatelliteView.h"
#include "ui/settings/SettingsView.h"
#include "ui/social-engineering/SocialEngineeringView.h"
#include "ui/unit-designer/UnitDesignerView.h"
#include "ui/world/CombatView.h"
#include "ui/world/WorldView.h"
#include <functional>
#include <memory>
#include <string>

namespace ac
{

class Faction;
class GameState;
class GameDataContext;
class GameSettings;
class Graphics;
class BaseManager;
class UnitSlotRegistry;
class WorldDisplay;
class Tile;

class ViewFactory
{
public:
    ViewFactory(
        GameState& rGameState,
        GameDataContext& rGameDataContext,
        Graphics& rGraphics,
        GameSettings& rSettings
    );

    std::unique_ptr<WorldView> CreateWorldView(
        const WindowLayout_t& layout,
        std::function<void()> onProcessTurn,
        std::function<void()> onRequestExit,
        std::function<void(BaseManager&)> onOpenBase,
        WorldView::OpenCombatCallback_t onOpenCombat,
        std::function<void()> onOpenCommlinks
    ) const;

    std::unique_ptr<BaseView> CreateBaseView(BaseManager& rBase) const;

    std::unique_ptr<BaseView> CreateBaseView(
        BaseManager& rBase,
        const WindowLayout_t& layout
    ) const;

    std::unique_ptr<ResearchView> CreateResearchView(
        const WindowLayout_t& layout
    ) const;

    std::unique_ptr<SocialEngineeringView> CreateSocialEngineeringView(
        const WindowLayout_t& layout
    ) const;

    std::unique_ptr<UnitDesignerView> CreateUnitDesignerView(
        const WindowLayout_t& layout
    ) const;

    std::unique_ptr<SettingsView> CreateSettingsView(
        const WindowLayout_t& layout
    ) const;

    std::unique_ptr<CommlinksView> CreateCommlinksView(
        const WindowLayout_t& layout,
        std::function<void()> onOpenCouncilVote
    ) const;

    std::unique_ptr<CouncilVoteView> CreateCouncilVoteView(
        const WindowLayout_t& layout
    ) const;

    std::unique_ptr<SatelliteView> CreateSatelliteView(
        const WindowLayout_t& layout
    ) const;

    std::unique_ptr<CombatView> CreateCombatView(
        const WindowLayout_t& layout,
        CombatResult_t result,
        const Tile& rAttackerTile,
        const Tile& rDefenderTile,
        std::string attackerName,
        std::string defenderName,
        WorldDisplay& rWorldDisplay,
        WindowLayout_t mapLayout,
        std::function<void()> onFinished
    ) const;

    WindowLayout_t GetFullscreenLayout() const;

private:
    // A view that needs the player faction cannot render anything meaningful without it, and
    // every caller reached here from a player action. Throws rather than returning a null view
    // for PushView to dereference.
    Faction* RequirePlayerFaction_() const;

    GameState& m_rGameState;
    GameDataContext& m_rGameDataContext;
    Graphics& m_rGraphics;
    GameSettings& m_rSettings;
};

} // namespace ac
