#pragma once

#include "game/diplomacy/DiplomacyLedger.h"
#include "game/base/BaseTypes.h"
#include <vector>

namespace ac
{

enum class DiplomaticActionKind_t
{
    ProposeTruce,
    ProposeTreaty,
    ProposePact,
    RenouncePact,
    Trade
};

// What SMAC's diplomacy menu offers face to face (terranx-map.md, Talk): the next status up,
// renouncing a Pact, and trade. Empty for a pair that has not met.
std::vector<DiplomaticActionKind_t> GetTalkActions(const DiplomacyLedger& rLedger,
                                                   FactionId_t a,
                                                   FactionId_t b);

} // namespace ac
