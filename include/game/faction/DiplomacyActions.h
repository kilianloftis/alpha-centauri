#pragma once

#include "game/faction/DiplomacyLedger.h"
#include "game/faction/TradeItem.h"
#include "game/faction/base/BaseTypes.h"
#include <string>
#include <vector>

namespace ac
{

enum class DiplomaticActionKind
{
    ProposeTruce,
    ProposeFriendship,
    ProposePact,
    DeclareVendetta,
    CancelTreaty,
    Trade
};

bool CanProposeTruce(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b);
bool CanProposeFriendship(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b);
bool CanProposePact(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b);
bool CanDeclareVendetta(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b);
bool CanCancelTreaty(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b);

// Whether this specific trade item is legal between a and b (known, not vendetta;
// bases and coordinated vendetta require Pact).
bool CanTrade(const DiplomacyLedger& rLedger,
              FactionId_t a,
              FactionId_t b,
              const TradeItem_t& rItem);

// Primary UI/AI entry: legal actions for the pair (empty if not known).
std::vector<DiplomaticActionKind> GetAvailableActions(const DiplomacyLedger& rLedger,
                                                      FactionId_t a,
                                                      FactionId_t b);

// Legal trade categories for the pair (empty under Vendetta / unknown).
std::vector<TradeKind_t> GetAvailableTrades(const DiplomacyLedger& rLedger,
                                          FactionId_t a,
                                          FactionId_t b);

std::string ToString(DiplomaticActionKind kind);
std::string ToString(TradeKind_t kind);

} // namespace ac
