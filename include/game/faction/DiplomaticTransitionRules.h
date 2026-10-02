#pragma once

#include "game/faction/DiplomaticStatus.h"
#include "game/faction/base/BaseTypes.h"

#include <optional>

namespace ac
{

class DiplomacyLedger;

// The status a pair may propose next. Any status may also go to Vendetta.
constexpr std::optional<DiplomaticStatus_t> StepUp(DiplomaticStatus_t status)
{
    switch (status)
    {
    case DiplomaticStatus_t::Neutral:
    case DiplomaticStatus_t::Truce:
        return DiplomaticStatus_t::Treaty;
    case DiplomaticStatus_t::Treaty:
        return DiplomaticStatus_t::Pact;
    case DiplomaticStatus_t::Vendetta:
        return DiplomaticStatus_t::Truce;
    case DiplomaticStatus_t::Pact:
        return std::nullopt;
    }
    return std::nullopt;
}

// The status a pair falls to when the agreement is canceled, broken, or expires.
constexpr std::optional<DiplomaticStatus_t> StepDown(DiplomaticStatus_t status)
{
    switch (status)
    {
    case DiplomaticStatus_t::Truce:
    case DiplomaticStatus_t::Treaty:
        return DiplomaticStatus_t::Neutral;
    case DiplomaticStatus_t::Pact:
        return DiplomaticStatus_t::Treaty;
    case DiplomaticStatus_t::Neutral:
    case DiplomaticStatus_t::Vendetta:
        return std::nullopt;
    }
    return std::nullopt;
}

// Whether a and b may propose StepUp of their current status.
bool CanProposeStepUp(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b);
bool CanDeclareVendetta(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b);
// Whether a and b hold a status with a StepDown to cancel into.
bool CanCancelTreaty(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b);

// Whether a may ask b to move their status to requested: StepUp of the current status,
// StepDown (a cancel), or Vendetta.
bool CanRequestStatus(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b,
                      DiplomaticStatus_t requested);

} // namespace ac
