#pragma once

#include "game/PauseOnEventsConfig.h"

#include <optional>
#include <string>

namespace ac
{

// Outcome of BaseManager::ApplyProduction (one mineral-banking tick).
enum class ProductionApplyKind_t
{
    Idle,                   // nothing queued
    InProgress,             // banked; cost not yet met
    Completed,              // item finished; completedId set
    AwaitingAbandonConfirm, // would empty the base; Confirm / Defer required
};

struct ProductionApplyResult_t
{
    ProductionApplyKind_t kind = ProductionApplyKind_t::Idle;
    std::string completedId;
    // Set when kind is Completed: facility vs combat/non-combat unit (for pause-on-event).
    std::optional<PauseOnEventId_t> completedEvent;
    std::string completedName;
};

} // namespace ac
