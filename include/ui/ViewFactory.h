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
struct GameDataContext;
class GameSettings;
class Graphics;
class BaseManager;
class UnitSlotRegistry;
class WorldDisplay;
class Tile;
class MapRenderer;
class HotkeyConfig;
class StandingCaptions;
class SpriteLibrary;

class ViewFactory
{
public:
    ViewFactory(
        GameState& rGameState,
        const GameDataContext& rGameDataContext,
        const HotkeyConfig& rHotkeys,
        const StandingCaptions& rStandingCaptions,
        Graphics& rGraphics,
        MapRenderer& rMapRenderer,
        SpriteLibrary& rSprites,
        GameSettings& rSettings
    );

    std::unique_ptr<WorldView> CreateWorldView(const WindowLayout_t& layout,
                                               WorldView::Callbacks_t callbacks) const;

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

    GameState& m_rGameState;
    const GameDataContext& m_rGameDataContext;
    const HotkeyConfig& m_rHotkeys;
    const StandingCaptions& m_rStandingCaptions;
    Graphics& m_rGraphics;
    MapRenderer& m_rMapRenderer;
    SpriteLibrary& m_rSprites;
    GameSettings& m_rSettings;
};

} // namespace ac
