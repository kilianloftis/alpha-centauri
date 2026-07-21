#pragma once

#include "game/faction/DiplomacyLedger.h"
#include "game/faction/base/BaseTypes.h"
#include "lib/GameEvent.h"
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ac
{

struct TradeCredits_t
{
    int amount = 0;
    std::string ToString() const;
};

struct TradeTechnology_t
{
    TechId techId;
    std::string ToString() const;
};

struct TradeBase_t
{
    BaseId_t baseId = 0;
    std::string ToString() const;
};

struct TradeCommFrequency_t
{
    FactionId_t factionId = 0;
    std::string ToString() const;
};

struct TradeWorldMap_t
{
    std::string ToString() const;
};

struct TradeDeclareVendetta_t
{
    FactionId_t againstFactionId = 0;
    std::string ToString() const;
};

using TradeItem_t = std::variant<TradeCredits_t,
                                 TradeTechnology_t,
                                 TradeBase_t,
                                 TradeCommFrequency_t,
                                 TradeWorldMap_t,
                                 TradeDeclareVendetta_t>;

std::string ToString(const TradeItem_t& rItem);

struct DiplomaticProposal_t
{
    FactionId_t proposer = 0;
    FactionId_t recipient = 0;
    // nullopt = keep current status; None = cancel treaty to no affiliation.
    std::optional<DiplomaticStatus> requestedStatus;
    std::vector<TradeItem_t> give;   // proposer → recipient
    std::vector<TradeItem_t> demand; // recipient → proposer
};

} // namespace ac
