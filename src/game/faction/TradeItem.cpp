#include "game/faction/TradeItem.h"

#include <sstream>

namespace ac
{

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

} // namespace ac
