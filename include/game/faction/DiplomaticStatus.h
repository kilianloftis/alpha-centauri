#pragma once

#include <magic_enum.hpp>

#include <cstddef>
#include <string>

namespace ac
{

// Pairwise diplomatic standing between two factions. Closed: config/diplomacy.json sets each
// status's rules, but game logic names these statuses and their transitions directly.
enum class DiplomaticStatus_t
{
    Neutral, // default for a pair with no agreement
    Truce,
    Treaty,
    Pact,
    Vendetta
};

inline constexpr std::size_t k_DiplomaticStatusCount = magic_enum::enum_count<DiplomaticStatus_t>();

// How a Vendetta began. A Declaration withdraws each side's units from the other's territory;
// a SneakAttack (a hostile act without one) does not. Defensive obligations inherit the entry
// of the Vendetta that raised them.
enum class VendettaEntry_t
{
    Declaration,
    SneakAttack
};

// Empty string for Neutral; otherwise the status name.
std::string ToString(DiplomaticStatus_t status);

} // namespace ac
