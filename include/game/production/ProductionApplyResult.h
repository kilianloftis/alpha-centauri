#pragma once

#include "game/PauseOnEventsConfig.h"

#include <optional>
#include <string>
#include <vector>

namespace ac
{

// Outcome of BaseManager::ApplyProduction / TryCompleteReadyProduction.
enum class ProductionApplyKind_t
{
    Idle,          // nothing queued and no available stockpile fallback
    // InProgress: cost not yet met, a stockpile convert path, riot, or deferred this turn.
    InProgress,

    Completed,     // item finished; completedId / completedEvents / completedName set
    // cost met, but finishing needs a player answer: CompletePendingProduction or
    // DeferProductionCompletion. The reason lives with the asker (BaseProduction / the UI),
    // not here — typically ProductionWouldEmptyInteraction.
    AwaitingConfirmation,
};

// Current production is no longer constructable. Minerals have already been banked onto it.
struct ProductionPreempt_t
{
    std::string droppedName;
};

struct ProductionApplyResult_t
{
    ProductionApplyKind_t kind = ProductionApplyKind_t::Idle;
    std::string completedId;
    // Set when kind is Completed: every applicable pause-on-event gate (OR-matched in the UI).
    std::vector<PauseOnEventId_t> completedEvents;
    std::string completedName;
    // Set when ValidateQueue dropped the current item. The upcoming queue is stripped silently.
    std::optional<ProductionPreempt_t> preempted;
};

} // namespace ac
