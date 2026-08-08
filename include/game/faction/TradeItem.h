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

// Categories of TradeItem_t the UI/AI can offer (relationship-gated, not ownership-gated).
enum class TradeKind_t
{
    Credits,
    Technology,
    Base,
    CommFrequency,
    WorldMap,
    DeclareVendetta
};

// One trait per alternative, next to the alternatives. A new TradeItem_t member without a
// specialisation fails to compile at the GetAvailableTrades fold rather than silently dropping
// out of the category list.
template <typename T>
struct TradeKindOf;

template <> struct TradeKindOf<TradeCredits_t>
{ static constexpr TradeKind_t value = TradeKind_t::Credits; };
template <> struct TradeKindOf<TradeTechnology_t>
{ static constexpr TradeKind_t value = TradeKind_t::Technology; };
template <> struct TradeKindOf<TradeBase_t>
{ static constexpr TradeKind_t value = TradeKind_t::Base; };
template <> struct TradeKindOf<TradeCommFrequency_t>
{ static constexpr TradeKind_t value = TradeKind_t::CommFrequency; };
template <> struct TradeKindOf<TradeWorldMap_t>
{ static constexpr TradeKind_t value = TradeKind_t::WorldMap; };
template <> struct TradeKindOf<TradeDeclareVendetta_t>
{ static constexpr TradeKind_t value = TradeKind_t::DeclareVendetta; };

// The kind of a live item — the same mapping, applied to a value rather than a type.
TradeKind_t KindOf(const TradeItem_t& rItem);

std::string ToString(const TradeItem_t& rItem);

struct DiplomaticProposal_t
{
    FactionId_t proposer = 0;
    FactionId_t recipient = 0;
    // nullopt = keep current status; None = cancel treaty to no affiliation.
    std::optional<DiplomaticStatus_t> requestedStatus;
    std::vector<TradeItem_t> give;   // proposer → recipient
    std::vector<TradeItem_t> demand; // recipient → proposer
};

} // namespace ac
