#include "ui/InteractionPresenter.h"

#include "game/faction/Faction.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/production/IConstructable.h"
#include "game/PauseOnEventsConfig.h"
#include "game/PlayerInteractionQueue.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/effects/TriggeredEffectRules.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticPermissionRules.h"
#include "game/diplomacy/DiplomaticTransitionMutations.h"
#include "game/diplomacy/DiplomaticTransitionRules.h"
#include "game/units/UnitManager.h"
#include "game/base/BaseManager.h"
#include "game/production/ProductionManager.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"
#include "game/movement/TerritoryEntryMutations.h"
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

    // Something owns the screen already. Only completing clears an item, so dismissing a prompt
    // with Escape re-opens it rather than stranding the turn.
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
            [this](const ProductionPreemptedInteraction_t& rPreempted) {
                PresentProductionPreempted_(rPreempted);
            },
            [this](const ImprovementVisitInteraction_t& rVisit) {
                PresentImprovementVisit_(rVisit);
            },
            [this](const ArtifactLinkInteraction_t& rLink) {
                PresentArtifactLink_(rLink);
            },
            [this](const PactObligationInteraction_t& rObligation) {
                PresentPactObligation_(rObligation);
            },
            [this](const TerritoryEntryInteraction_t& rEntry) { PresentTerritoryEntry_(rEntry); },
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
    const WindowLayout_t layout = m_rWorldView.GetPopupLayout();
    auto pPopup = std::make_unique<NoticePopup>(
        layout, rNotice.title, rNotice.body, [this]() { CompleteAndAdvance_(); }, nullptr);
    m_rWorldView.PushModal(std::move(pPopup));
}

void InteractionPresenter::PresentOpenView_(const OpenViewInteraction_t& rOpen)
{
    auto onClosed = [this]() { CompleteAndAdvance_(); };
    const WindowLayout_t fullscreen = m_rViews.GetFullscreenLayout();

    switch (rOpen.view)
    {
    case OpenViewInteraction_t::View_t::Base:
    {
        Faction& rPlayer = m_rGameState.GetPlayerFaction();
        BaseManager* pBase = rOpen.baseId.has_value() ? rPlayer.FindBase(*rOpen.baseId) : nullptr;
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

    // The base, if it is still there and still waiting for this answer.
    auto pendingBase = [this, baseId, factionId]() -> BaseManager*
    {
        BaseManager* pBase = FindAudienceBase_(factionId, baseId);
        return pBase && pBase->HasPendingProductionConfirmation() ? pBase : nullptr;
    };

    std::vector<PopupChoice_t> choices;
    choices.push_back({"Complete " + itemName + " anyway", [this, pendingBase] {
                           if (BaseManager* pResolve = pendingBase())
                           {
                               pResolve->CompletePendingProduction();
                           }
                           CompleteAndAdvance_();
                       }});
    choices.push_back({"Not this turn", [this, pendingBase] {
                           if (BaseManager* pResolve = pendingBase())
                           {
                               pResolve->DeferProductionCompletion();
                           }
                           CompleteAndAdvance_();
                       }});
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

    const std::string title = rIdle.afterCompletion
        ? ("Base '" + pBase->GetName() + "' completed " + rIdle.completedItemName + ".")
        : ("No production at " + pBase->GetName());
    PresentContinueOrZoomBase_(*pBase, title);
}

void InteractionPresenter::PresentProductionPreempted_(
    const ProductionPreemptedInteraction_t& rPreempted)
{
    BaseManager* pBase = FindAudienceBase_(rPreempted.factionId, rPreempted.baseId);
    if (!pBase)
    {
        CompleteAndAdvance_();
        return;
    }

    const PauseOnEventsConfig_t& rPause = m_rGameState.GetSettings().GetPauseOnEvents();
    if (!rPause.Allows(PauseOnEventId_t::BuildOrdersOutOfDate))
    {
        CompleteAndAdvance_();
        return;
    }

    const std::string title = "Base '" + pBase->GetName() + "' can no longer produce "
                             + rPreempted.droppedItemName + ".";
    PresentContinueOrZoomBase_(*pBase, title);
}

void InteractionPresenter::PresentContinueOrZoomBase_(const BaseManager& rBase, std::string title)
{
    FocusBase_(rBase);

    const BaseId_t baseId = rBase.GetBaseId();
    const FactionId_t factionId = rBase.GetFactionId();
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
                           auto pView = m_rViews.CreateBaseView(*pOpen);
                           OpenView_(std::move(pView), [this] { m_onAdvance(); });
                       }});
    PushChoice_(std::move(title), std::move(choices));
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
    rTile.ForEachOccupant([&](const ImprovementConfig_t& rHost)
    {
        if (rHost.onVisitEffects.empty())
        {
            return false;
        }
        hostName = rHost.name.empty() ? rHost.id : rHost.name;
        return true;
    });

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
    const std::string hostName = pUnit ? HoldLinkHostName(*pUnit) : std::string{};
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

void InteractionPresenter::PresentPactObligation_(const PactObligationInteraction_t& rObligation)
{
    const Faction& rPlayer = m_rGameState.GetPlayerFaction();
    const Faction* pAlly = m_rGameState.FindFaction(rObligation.allyId);
    const Faction* pAggressor = m_rGameState.FindFaction(rObligation.aggressorId);
    if (!pAlly || !pAggressor
        || !IsObligedToDefend(rPlayer.GetFactionId(), rObligation.allyId,
                              rObligation.aggressorId))
    {
        CompleteAndAdvance_();
        return;
    }

    const FactionId_t playerId = rPlayer.GetFactionId();
    const FactionId_t allyId = rObligation.allyId;
    const FactionId_t aggressorId = rObligation.aggressorId;
    const VendettaKind_t kind = rObligation.kind;
    const DiplomaticStatus_t status = m_rGameState.GetDiplomacyLedger().GetStatus(playerId, allyId);
    const std::string& rAllyName = pAlly->GetDefinition().identity.name;
    const std::string& rAggressorName = pAggressor->GetDefinition().identity.name;

    std::vector<PopupChoice_t> choices;
    choices.push_back(
        {"Declare Vendetta on the " + rAggressorName,
         [this, playerId, aggressorId, kind]
         {
             HonorDefensiveObligation(m_rGameState, playerId, aggressorId, kind);
             CompleteAndAdvance_();
         }});
    choices.push_back(
        {"Stand aside (" + ToString(status) + " with the " + rAllyName + " becomes "
             + ToString(StepDown(status).value()) + ")",
         [this, playerId, allyId]
         {
             DeclineDefensiveObligation(m_rGameState, playerId, allyId);
             CompleteAndAdvance_();
         }});
    PushChoice_("The " + rAggressorName + " attacked the " + rAllyName + ", our "
                    + ToString(status) + " partner.",
                std::move(choices));
}

void InteractionPresenter::PresentTerritoryEntry_(const TerritoryEntryInteraction_t& rEntry)
{
    Unit* pUnit = FindUnit_(rEntry.unitId);
    const Faction* pOwner = m_rGameState.FindFaction(rEntry.ownerId);
    if (!pUnit || !pOwner || !pUnit->GetOrder().has_value())
    {
        CompleteAndAdvance_();
        return;
    }

    const Tile& rTile = pUnit->GetTile();
    m_rWorldView.CenterOnTile(rTile.GetX(), rTile.GetY());

    const UnitId_t unitId = rEntry.unitId;
    const FactionId_t ownerId = rEntry.ownerId;
    const std::string status = ToString(m_rGameState.GetDiplomacyLedger().GetStatus(
        pUnit->GetFaction().GetFactionId(), ownerId));
    const std::string& rOwnerName = pOwner->GetDefinition().identity.name;

    std::vector<PopupChoice_t> choices;
    choices.push_back(
        {"Break the " + status + " (Vendetta with the " + rOwnerName + ")",
         [this, unitId, ownerId]
         {
             if (Unit* pResolve = FindUnit_(unitId))
             {
                 BreakAgreementAndContinue(m_rGameState, *pResolve, ownerId);
             }
             CompleteAndAdvance_();
         }});
    choices.push_back(
        {"Cancel the order",
         [this, unitId]
         {
             if (Unit* pResolve = FindUnit_(unitId))
             {
                 pResolve->ClearOrder();
             }
             CompleteAndAdvance_();
         }});
    PushChoice_("Our " + status + " with the " + rOwnerName + " forbids entering their territory.",
                std::move(choices));
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
    const WindowLayout_t layout = m_rWorldView.GetPopupLayout();
    auto pPopup = std::make_unique<ListSelectorPopup>(
        std::move(title), "No options", std::move(choices), layout, Style().listSelectorPopup,
        nullptr);
    m_rWorldView.PushModal(std::move(pPopup));
}

void InteractionPresenter::OpenView_(std::unique_ptr<IGameView> pView,
                                     std::function<void()> onClosed)
{
    m_rUi.PushView(std::move(pView));
    m_onOverlayClosed = std::move(onClosed);
}

} // namespace ac
