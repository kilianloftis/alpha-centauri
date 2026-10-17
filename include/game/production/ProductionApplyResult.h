#pragma once

#include "game/PauseOnEventsConfig.h"

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

struct ProductionApplyResult_t
{
    ProductionApplyKind_t kind = ProductionApplyKind_t::Idle;
    std::string completedId;
    // Set when kind is Completed: every applicable pause-on-event gate (OR-matched in the UI).
    std::vector<PauseOnEventId_t> completedEvents;
    std::string completedName;
};

} // namespace ac
