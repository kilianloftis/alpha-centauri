#include "ui/ViewFactory.h"
#include "game/GameState.h"
#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/Military.h"
#include "game/faction/ResearchManager.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitSlotRegistry.h"
#include "graphics/Graphics.h"

namespace ac
{

ViewFactory::ViewFactory(
    GameState& rGameState,
    GameDataContext& rGameDataContext,
    Graphics& rGraphics
)
    : m_rGameState(rGameState)
    , m_rGameDataContext(rGameDataContext)
    , m_rGraphics(rGraphics)
{
}

std::unique_ptr<WorldView> ViewFactory::CreateWorldView(
    const WindowLayout_t& layout,
    std::function<void()> onProcessTurn,
    std::function<void()> onRequestExit,
    std::function<void(BaseManager&)> onOpenBase
) const
{
    const WorldMap* pWorldMap = m_rGameState.GetWorldMap();
    if (!pWorldMap)
    {
        return nullptr;
    }

    return std::make_unique<WorldView>(
        m_rGameState,
        *pWorldMap,
        layout,
        std::move(onProcessTurn),
        std::move(onRequestExit),
        std::move(onOpenBase));
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
    const Faction* pFaction = m_rGameState.GetPlayerFaction();
    if (!pFaction)
    {
        return nullptr;
    }

    return std::make_unique<BaseView>(rBase, *pFaction, layout);
}

std::unique_ptr<ResearchView> ViewFactory::CreateResearchView(
    const WindowLayout_t& layout
) const
{
    const Faction* pFaction = m_rGameState.GetPlayerFaction();
    if (!pFaction)
    {
        return nullptr;
    }

    return std::make_unique<ResearchView>(pFaction->GetResearchManager(), layout);
}

std::unique_ptr<UnitDesignerView> ViewFactory::CreateUnitDesignerView(
    const WindowLayout_t& layout
) const
{
    Faction* pFaction = m_rGameState.GetPlayerFaction();
    if (!pFaction)
    {
        return nullptr;
    }

    return std::make_unique<UnitDesignerView>(
        pFaction->GetMilitary(),
        *m_rGameDataContext.unitComponentRegistry,
        *m_rGameDataContext.unitSlotRegistry,
        nullptr, // TODO: pass UnitManager once wired into Faction
        layout
    );
}

WindowLayout_t ViewFactory::GetFullscreenLayout() const
{
    return {
        0.0f,
        0.0f,
        static_cast<float>(m_rGraphics.GetWindowWidth()),
        static_cast<float>(m_rGraphics.GetWindowHeight())
    };
}

} // namespace ac
