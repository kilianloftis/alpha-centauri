#pragma once

#include "game/PauseOnEventsConfig.h"

#include <optional>
#include <string>

namespace ac
{

// Outcome of BaseManager::ApplyProduction / TryCompleteReadyProduction.
enum class ProductionApplyKind_t
{
    Idle,          // nothing queued and no available stockpile fallback
    InProgress,    // cost not yet met (or a never-completing stockpile)

    Completed,     // item finished; completedId set
    WouldEmptyBase, // cost met; CompletePendingProduction / DisableProduction required
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
