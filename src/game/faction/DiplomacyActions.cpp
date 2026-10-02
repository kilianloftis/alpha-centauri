#include "game/faction/DiplomacyActions.h"

#include "game/faction/DiplomaticTransitionRules.h"

#include <stdexcept>

namespace ac
{

namespace
{

DiplomaticActionKind_t ProposeKindFor_(DiplomaticStatus_t target)
{
    switch (target)
    {
    case DiplomaticStatus_t::Truce:
        return DiplomaticActionKind_t::ProposeTruce;
    case DiplomaticStatus_t::Treaty:
        return DiplomaticActionKind_t::ProposeTreaty;
    case DiplomaticStatus_t::Pact:
        return DiplomaticActionKind_t::ProposePact;
    case DiplomaticStatus_t::Neutral:
    case DiplomaticStatus_t::Vendetta:
        break;
    }
    throw std::logic_error("ProposeKindFor_: no proposal leads to this status");
}

} // namespace

std::vector<DiplomaticActionKind_t> GetAvailableActions(const DiplomacyLedger& rLedger,
                                                      FactionId_t a,
                                                      FactionId_t b)
{
    std::vector<DiplomaticActionKind_t> actions;
    if (CanProposeStepUp(rLedger, a, b))
    {
        actions.push_back(ProposeKindFor_(*StepUp(rLedger.GetStatus(a, b))));
    }
    if (CanDeclareVendetta(rLedger, a, b))
    {
        actions.push_back(DiplomaticActionKind_t::DeclareVendetta);
    }
    if (CanCancelTreaty(rLedger, a, b))
    {
        actions.push_back(DiplomaticActionKind_t::CancelTreaty);
    }
    if (rLedger.AreKnown(a, b))
    {
        actions.push_back(DiplomaticActionKind_t::Trade);
    }
    return actions;
}

std::string ToString(DiplomaticActionKind_t kind)
{
    switch (kind)
    {
    case DiplomaticActionKind_t::ProposeTruce:
        return "Propose Truce";
    case DiplomaticActionKind_t::ProposeTreaty:
        return "Propose Treaty";
    case DiplomaticActionKind_t::ProposePact:
        return "Propose Pact";
    case DiplomaticActionKind_t::DeclareVendetta:
        return "Declare Vendetta";
    case DiplomaticActionKind_t::CancelTreaty:
        return "Cancel Treaty";
    case DiplomaticActionKind_t::Trade:
        return "Trade";
    }
    throw std::runtime_error("ToString: unhandled DiplomaticActionKind_t");
}

} // namespace ac
