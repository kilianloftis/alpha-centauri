#pragma once

#include "game/faction/DiplomacyLedger.h"
#include "game/faction/base/BaseTypes.h"
#include <string>
#include <vector>

namespace ac
{

enum class DiplomaticActionKind_t
{
    ProposeTruce,
    ProposeTreaty,
    ProposePact,
    DeclareVendetta,
    CancelTreaty,
    Trade
};

// Primary UI/AI entry: legal actions for the pair (empty if not known).
std::vector<DiplomaticActionKind_t> GetAvailableActions(const DiplomacyLedger& rLedger,
                                                      FactionId_t a,
                                                      FactionId_t b);

std::string ToString(DiplomaticActionKind_t kind);

} // namespace ac
