#pragma once

#include "game/PlayerInteraction.h"
#include "ui/ListSelectorPopup.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ac
{

class BaseManager;
class GameState;
class IGameView;
class UIManager;
class ViewFactory;
class WorldView;

// Engine-owned coordinator: maps PlayerInteractionQueue::Front to Notice / OpenView (and
// production-typed arms). Completing an interaction calls CompleteFront then onAdvance
// (Engine::ProcessTurn_).
class InteractionPresenter
{
public:
    InteractionPresenter(GameState& rGameState,
                         UIManager& rUi,
                         ViewFactory& rViews,
                         WorldView& rWorldView,
                         std::function<void()> onAdvance);

    // Poll Front each UI frame. No-op while a modal or overlay owns the screen — including
    // the one this presenter put there.
    void Update();

private:
    void PresentFront_(const PlayerInteraction_t& rPayload);
    void CompleteAndAdvance_();

    void PresentNotice_(const NoticeInteraction_t& rNotice);
    void PresentOpenView_(const OpenViewInteraction_t& rOpen);
    void PresentProductionWouldEmpty_(const ProductionWouldEmptyInteraction_t& rWouldEmpty);
    void PresentProductionIdle_(const ProductionIdleInteraction_t& rIdle);

    BaseManager* FindAudienceBase_(FactionId_t factionId, BaseId_t baseId);
    void FocusBase_(const BaseManager& rBase);
    void PushChoice_(std::string title, std::vector<PopupChoice_t> choices);
    // Push a full-screen overlay; onClosed runs once the overlay stack empties again.
    void OpenView_(std::unique_ptr<IGameView> pView, std::function<void()> onClosed);

    GameState& m_rGameState;
    UIManager& m_rUi;
    ViewFactory& m_rViews;
    WorldView& m_rWorldView;
    std::function<void()> m_onAdvance;

    // Set while an overlay this presenter pushed is open; runs when the stack empties.
    std::function<void()> m_onOverlayClosed;
};

} // namespace ac
