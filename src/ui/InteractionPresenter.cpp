#include "ui/InteractionPresenter.h"

#include "game/Faction.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/IConstructable.h"
#include "game/PauseOnEventsConfig.h"
#include "game/PlayerInteractionQueue.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"
#include "game/units/Unit.h"
#include "ui/ListSelectorPopup.h"
#include "ui/NoticePopup.h"
#include "ui/UIManager.h"
#include "ui/ViewFactory.h"
#include "ui/style/UiStyle.h"
#include "ui/world/WorldView.h"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <variant>

namespace ac
{

namespace
{

// Visitor overload set: an unhandled PlayerInteraction_t alternative has no matching
// operator() and fails to compile, which an if-constexpr chain would not.
template <typename... Ts>
struct Overloaded : Ts...
{
    using Ts::operator()...;
};
template <typename... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

} // namespace

InteractionPresenter::InteractionPresenter(GameState& rGameState,
                                           UIManager& rUi,
                                           ViewFactory& rViews,
                                           WorldView& rWorldView,
                                           std::function<void()> onAdvance)
    : m_rGameState(rGameState)
    , m_rUi(rUi)
    , m_rViews(rViews)
    , m_rWorldView(rWorldView)
    , m_onAdvance(std::move(onAdvance))
{
    if (!m_onAdvance)
    {
        throw std::invalid_argument("InteractionPresenter requires an onAdvance callback");
    }
}

void InteractionPresenter::Update()
{
    if (m_onOverlayClosed)
    {
        if (m_rUi.HasOverlayView())
        {
            return;
        }
        const std::function<void()> onClosed = std::move(m_onOverlayClosed);
        m_onOverlayClosed = nullptr;
        onClosed();
        return;
    }

    const QueuedInteraction_t* pFront = m_rGameState.GetPlayerInteractions().Front();
    if (!pFront)
    {
        return;
    }

    // Something already owns the screen: either the prompt for this item, or an unrelated
    // overlay / in-view modal this must not stack on top of. Once the screen is free the
    // front is presented again — a queued item is only cleared by completing it, so
    // dismissing a prompt with Escape re-opens it rather than stranding the turn.
    if (m_rUi.HasOverlayView() || m_rWorldView.HasModalElement())
    {
        return;
    }

    // Presenting can complete the front (missing base, immediate resolve), which destroys the
    // queued item, so visit a copy rather than the payload still owned by the queue.
    const PlayerInteraction_t payload = pFront->payload;
    PresentFront_(payload);
}

void InteractionPresenter::PresentFront_(const PlayerInteraction_t& rPayload)
{
    std::visit(
        Overloaded{
            [this](const NoticeInteraction_t& rNotice) { PresentNotice_(rNotice); },
            [this](const OpenViewInteraction_t& rOpen) { PresentOpenView_(rOpen); },
            [this](const ProductionWouldEmptyInteraction_t& rWouldEmpty) {
                PresentProductionWouldEmpty_(rWouldEmpty);
            },
            [this](const ProductionIdleInteraction_t& rIdle) { PresentProductionIdle_(rIdle); },
            [this](const ImprovementVisitInteraction_t& rVisit) {
                PresentImprovementVisit_(rVisit);
            },
            [this](const ArtifactLinkInteraction_t& rLink) {
                PresentArtifactLink_(rLink);
            },
        },
        rPayload);
}

void InteractionPresenter::CompleteAndAdvance_()
{
    m_rGameState.GetPlayerInteractions().CompleteFront();
    m_onAdvance();
}

void InteractionPresenter::PresentNotice_(const NoticeInteraction_t& rNotice)
{
    if (!m_rGameState.GetSettings().GetPauseOnEvents().Allows(rNotice.event))
    {
        CompleteAndAdvance_();
        return;
    }

    if (rNotice.cameraTile.has_value())
    {
        m_rWorldView.CenterOnTile(rNotice.cameraTile->first, rNotice.cameraTile->second);
    }
    m_rWorldView.PushModal(std::make_unique<NoticePopup>(
        m_rWorldView.GetPopupLayout(),
        rNotice.title,
        rNotice.body,
        [this]() { CompleteAndAdvance_(); }));
}

void InteractionPresenter::PresentOpenView_(const OpenViewInteraction_t& rOpen)
{
    auto onClosed = [this]() { CompleteAndAdvance_(); };
    const WindowLayout_t fullscreen = m_rViews.GetFullscreenLayout();

    switch (rOpen.view)
    {
    case OpenViewInteraction_t::View_t::Base:
    {
        Faction* pPlayer = m_rGameState.GetPlayerFaction();
        BaseManager* pBase = (pPlayer && rOpen.baseId.has_value())
            ? pPlayer->FindBase(*rOpen.baseId)
            : nullptr;
        if (!pBase)
        {
            CompleteAndAdvance_();
            return;
        }
        OpenView_(m_rViews.CreateBaseView(*pBase), std::move(onClosed));
        return;
    }
    case OpenViewInteraction_t::View_t::Research:
        OpenView_(m_rViews.CreateResearchView(fullscreen), std::move(onClosed));
        return;
    case OpenViewInteraction_t::View_t::UnitDesigner:
        OpenView_(m_rViews.CreateUnitDesignerView(fullscreen), std::move(onClosed));
        return;
    case OpenViewInteraction_t::View_t::SocialEngineering:
        OpenView_(m_rViews.CreateSocialEngineeringView(fullscreen), std::move(onClosed));
        return;
    }
}

void InteractionPresenter::PresentProductionWouldEmpty_(
    const ProductionWouldEmptyInteraction_t& rWouldEmpty)
{
    BaseManager* pBase = FindAudienceBase_(rWouldEmpty.factionId, rWouldEmpty.baseId);
    if (!pBase || !pBase->HasPendingProductionConfirmation())
    {
        CompleteAndAdvance_();
        return;
    }

    FocusBase_(*pBase);

    std::string itemName = "production";
    if (const IConstructable* pItem = pBase->GetProduction().GetCurrentProduction())
    {
        itemName = pItem->GetName();
    }
    const BaseId_t baseId = rWouldEmpty.baseId;
    const FactionId_t factionId = rWouldEmpty.factionId;

    auto resolveChoice = [this, baseId, factionId](bool bComplete)
    {
        BaseManager* pResolve = FindAudienceBase_(factionId, baseId);
        if (pResolve && pResolve->HasPendingProductionConfirmation())
        {
            if (bComplete)
            {
                pResolve->CompletePendingProduction();
            }
            else
            {
                pResolve->DeferProductionCompletion();
            }
        }
        CompleteAndAdvance_();
    };

    std::vector<PopupChoice_t> choices;
    choices.push_back({"Complete " + itemName + " anyway",
                       [resolveChoice] { resolveChoice(true); }});
    choices.push_back({"Not this turn", [resolveChoice] { resolveChoice(false); }});
    PushChoice_("Completing this would empty " + pBase->GetName() + ".", std::move(choices));
}

void InteractionPresenter::PresentProductionIdle_(const ProductionIdleInteraction_t& rIdle)
{
    BaseManager* pBase = FindAudienceBase_(rIdle.factionId, rIdle.baseId);
    if (!pBase)
    {
        CompleteAndAdvance_();
        return;
    }

    const PauseOnEventsConfig_t& rPause = m_rGameState.GetSettings().GetPauseOnEvents();
    const bool bAllowed = rIdle.completedEvents.empty()
        ? rPause.Allows(PauseOnEventId_t::BuildOrdersOutOfDate)
        : std::any_of(rIdle.completedEvents.begin(), rIdle.completedEvents.end(),
                      [&rPause](PauseOnEventId_t gate) { return rPause.Allows(gate); });
    if (!bAllowed)
    {
        CompleteAndAdvance_();
        return;
    }

    FocusBase_(*pBase);

    const std::string title = rIdle.afterCompletion
        ? ("Base '" + pBase->GetName() + "' completed " + rIdle.completedItemName + ".")
        : ("No production at " + pBase->GetName());
    const BaseId_t baseId = rIdle.baseId;
    const FactionId_t factionId = rIdle.factionId;

    std::vector<PopupChoice_t> choices;
    choices.push_back({"Continue", [this] { CompleteAndAdvance_(); }});
    choices.push_back({"Zoom to base control",
                       [this, baseId, factionId]
                       {
                           m_rGameState.GetPlayerInteractions().CompleteFront();
                           BaseManager* pOpen = FindAudienceBase_(factionId, baseId);
                           if (!pOpen)
                           {
                               m_onAdvance();
                               return;
                           }
                           OpenView_(m_rViews.CreateBaseView(*pOpen), [this] { m_onAdvance(); });
                       }});
    PushChoice_(title, std::move(choices));
}

void InteractionPresenter::PresentImprovementVisit_(const ImprovementVisitInteraction_t& rVisit)
{
    Unit* pUnit = FindUnit_(rVisit.unitId);
    if (!pUnit || !TileHasVisitEffects(pUnit->GetTile()))
    {
        CompleteAndAdvance_();
        return;
    }

    const Tile& rTile = pUnit->GetTile();
    m_rWorldView.CenterOnTile(rTile.GetX(), rTile.GetY());

    std::string hostName = "site";
    for (const ImprovementConfig_t* pImprovement : rTile.GetImprovements())
    {
        if (pImprovement && !pImprovement->onVisitEffects.empty())
        {
            hostName = pImprovement->name.empty() ? pImprovement->id : pImprovement->name;
            break;
        }
    }

    const UnitId_t unitId = rVisit.unitId;
    std::vector<PopupChoice_t> choices;
    choices.push_back(
        {"Investigate the " + hostName,
         [this, unitId]
         {
             if (Unit* pResolve = FindUnit_(unitId))
             {
                 ApplyVisitEffects(m_rGameState, *pResolve, m_rGameState.GetRng());
             }
             CompleteAndAdvance_();
         }});
    choices.push_back({"Leave it Alone", [this] { CompleteAndAdvance_(); }});
    PushChoice_("A " + hostName + " stands here.", std::move(choices));
}

void InteractionPresenter::PresentArtifactLink_(const ArtifactLinkInteraction_t& rLink)
{
    Unit* pUnit = FindUnit_(rLink.unitId);
    const std::string hostName = pUnit ? HoldLinkHostName(m_rGameState, *pUnit) : std::string{};
    if (!pUnit || hostName.empty())
    {
        CompleteAndAdvance_();
        return;
    }

    const Tile& rTile = pUnit->GetTile();
    m_rWorldView.CenterOnTile(rTile.GetX(), rTile.GetY());

    const std::string unitName = pUnit->GetDesign().GetName();
    const UnitId_t unitId = rLink.unitId;
    std::vector<PopupChoice_t> choices;
    choices.push_back(
        {"Link the " + unitName + " to the " + hostName,
         [this, unitId]
         {
             if (Unit* pResolve = FindUnit_(unitId))
             {
                 ApplyHoldLink(m_rGameState, *pResolve);
             }
             CompleteAndAdvance_();
         }});
    choices.push_back({"Do nothing", [this] { CompleteAndAdvance_(); }});
    PushChoice_("Link the " + unitName + " to the " + hostName + "?", std::move(choices));
}

BaseManager* InteractionPresenter::FindAudienceBase_(FactionId_t factionId, BaseId_t baseId)
{
    Faction* pFaction = m_rGameState.FindFaction(factionId);
    if (!pFaction)
    {
        return nullptr;
    }
    return pFaction->FindBase(baseId);
}

Unit* InteractionPresenter::FindUnit_(UnitId_t unitId)
{
    for (Faction& rFaction : m_rGameState.Factions())
    {
        for (Unit& rUnit : rFaction.GetUnitManager().Units())
        {
            if (rUnit.GetUnitId() == unitId)
            {
                return &rUnit;
            }
        }
    }
    return nullptr;
}

void InteractionPresenter::FocusBase_(const BaseManager& rBase)
{
    const Tile& rTile = rBase.GetTile();
    m_rWorldView.CenterOnTile(rTile.GetX(), rTile.GetY());
}

void InteractionPresenter::PushChoice_(std::string title, std::vector<PopupChoice_t> choices)
{
    m_rWorldView.PushModal(std::make_unique<ListSelectorPopup>(
        std::move(title),
        "No options",
        std::move(choices),
        m_rWorldView.GetPopupLayout(),
        Style().listSelectorPopup));
}

void InteractionPresenter::OpenView_(std::unique_ptr<IGameView> pView,
                                     std::function<void()> onClosed)
{
    m_rUi.PushView(std::move(pView));
    m_onOverlayClosed = std::move(onClosed);
}

} // namespace ac
