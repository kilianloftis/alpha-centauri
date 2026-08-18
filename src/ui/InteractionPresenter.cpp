#include "ui/InteractionPresenter.h"

#include "game/Faction.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/IConstructable.h"
#include "game/PauseOnEventsConfig.h"
#include "game/PlayerInteractionQueue.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/map/Tile.h"
#include "ui/ListSelectorPopup.h"
#include "ui/NoticePopup.h"
#include "ui/UIManager.h"
#include "ui/ViewFactory.h"
#include "ui/style/UiStyle.h"
#include "ui/world/WorldView.h"

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
            [this](const ProductionAbandonInteraction_t& rAbandon) {
                PresentProductionAbandon_(rAbandon);
            },
            [this](const ProductionIdleInteraction_t& rIdle) { PresentProductionIdle_(rIdle); },
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

void InteractionPresenter::PresentProductionAbandon_(const ProductionAbandonInteraction_t& rAbandon)
{
    BaseManager* pBase = FindAudienceBase_(rAbandon.factionId, rAbandon.baseId);
    if (!pBase || !pBase->HasPendingProductionAbandonConfirm())
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
    const BaseId_t baseId = rAbandon.baseId;
    const FactionId_t factionId = rAbandon.factionId;

    auto resolveAbandon = [this, baseId, factionId](bool bConfirm)
    {
        BaseManager* pResolve = FindAudienceBase_(factionId, baseId);
        if (pResolve && pResolve->HasPendingProductionAbandonConfirm())
        {
            if (bConfirm)
            {
                pResolve->ConfirmProductionAbandon();
            }
            else
            {
                pResolve->DeferProductionAbandon();
            }
        }
        CompleteAndAdvance_();
    };

    std::vector<PopupChoice_t> choices;
    choices.push_back({"Complete " + itemName + " (abandon base)",
                       [resolveAbandon] { resolveAbandon(true); }});
    choices.push_back({"Defer (lose minerals)", [resolveAbandon] { resolveAbandon(false); }});
    PushChoice_("Abandon " + pBase->GetName() + "?", std::move(choices));
}

void InteractionPresenter::PresentProductionIdle_(const ProductionIdleInteraction_t& rIdle)
{
    BaseManager* pBase = FindAudienceBase_(rIdle.factionId, rIdle.baseId);
    if (!pBase)
    {
        CompleteAndAdvance_();
        return;
    }

    const PauseOnEventId_t gate = rIdle.completedEvent.value_or(
        PauseOnEventId_t::BuildOrdersOutOfDate);
    if (!m_rGameState.GetSettings().GetPauseOnEvents().Allows(gate))
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

BaseManager* InteractionPresenter::FindAudienceBase_(FactionId_t factionId, BaseId_t baseId)
{
    Faction* pFaction = m_rGameState.FindFaction(factionId);
    if (!pFaction)
    {
        return nullptr;
    }
    return pFaction->FindBase(baseId);
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
