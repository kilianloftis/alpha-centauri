#pragma once

#include "game/faction/DiplomaticStatus.h"
#include "game/faction/base/BaseTypes.h"

#include <optional>

namespace ac
{

class DiplomacyLedger;
class Unit;

// aggressor acted against victim (combat, bombardment, a probe action, an atrocity). Built
// before the act resolves: either unit may not survive it.
struct HostileAct_t
{
    FactionId_t aggressor;
    FactionId_t victim;
    // Each owner knows whose unit the other is.
    bool bAttributed;
};

// rActor's act against rTarget's owner. A covert unit on either side leaves it unattributed.
HostileAct_t HostileActAgainst(const Unit& rActor, const Unit& rTarget);

// rActor's act against victim's bases or improvements. Attributed when victim knows whose unit
// rActor is.
HostileAct_t HostileActAgainst(const Unit& rActor, FactionId_t victim);

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

} // namespace ac
