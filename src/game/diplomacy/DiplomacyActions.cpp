#include "game/diplomacy/DiplomacyActions.h"

#include "game/diplomacy/DiplomaticTransitionRules.h"

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

std::vector<DiplomaticActionKind_t> GetTalkActions(const DiplomacyLedger& rLedger,
                                                   FactionId_t a,
                                                   FactionId_t b)
{
    std::vector<DiplomaticActionKind_t> actions;
    if (!rLedger.AreKnown(a, b))
    {
        return actions;
    }
    const DiplomaticStatus_t status = rLedger.GetStatus(a, b);
    if (CanProposeStepUp(rLedger, a, b))
    {
        actions.push_back(ProposeKindFor_(StepUp(status).value()));
    }
    if (status == DiplomaticStatus_t::Pact)
    {
        actions.push_back(DiplomaticActionKind_t::RenouncePact);
    }
    actions.push_back(DiplomaticActionKind_t::Trade);
    return actions;
}

} // namespace ac
