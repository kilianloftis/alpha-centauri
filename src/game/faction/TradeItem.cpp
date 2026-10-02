#include "game/faction/TradeItem.h"

#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace ac
{

namespace
{

template <std::size_t... I>
std::vector<TradeKind_t> TradeKindsOf_(std::index_sequence<I...>)
{
    return {TradeKindOf<std::variant_alternative_t<I, TradeItem_t>>::value...};
}

} // namespace

std::string TradeCredits_t::ToString() const
{
    return "Credits(" + std::to_string(amount) + ")";
}

std::string TradeTechnology_t::ToString() const
{
    return "Technology(" + techId + ")";
}

std::string TradeBase_t::ToString() const
{
    return "Base(" + std::to_string(baseId) + ")";
}

std::string TradeCommFrequency_t::ToString() const
{
    return "CommFrequency(" + std::to_string(factionId) + ")";
}

std::string TradeWorldMap_t::ToString() const
{
    return "WorldMap";
}

std::string TradeDeclareVendetta_t::ToString() const
{
    return "DeclareVendetta(" + std::to_string(againstFactionId) + ")";
}

std::string ToString(const TradeItem_t& rItem)
{
    return std::visit([](const auto& rConcrete) { return rConcrete.ToString(); }, rItem);
}

std::vector<TradeKind_t> TradeKinds()
{
    return TradeKindsOf_(std::make_index_sequence<std::variant_size_v<TradeItem_t>>{});
}

std::string ToString(TradeKind_t kind)
{
    // Display labels insert spaces the enumerator names do not carry.
    switch (kind)
    {
    case TradeKind_t::Credits:
        return "Credits";
    case TradeKind_t::Technology:
        return "Technology";
    case TradeKind_t::Base:
        return "Base";
    case TradeKind_t::CommFrequency:
        return "Comm Frequency";
    case TradeKind_t::WorldMap:
        return "World Map";
    case TradeKind_t::DeclareVendetta:
        return "Declare Vendetta";
    }
    throw std::runtime_error("ToString: unhandled TradeKind_t");
}

} // namespace ac
