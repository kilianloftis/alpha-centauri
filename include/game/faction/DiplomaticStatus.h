#pragma once

#include <magic_enum.hpp>

#include <cstddef>
#include <optional>
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

// The status a pair may propose next. Any status may also go to Vendetta.
constexpr std::optional<DiplomaticStatus_t> StepUp(DiplomaticStatus_t status)
{
    switch (status)
    {
    case DiplomaticStatus_t::Neutral:
    case DiplomaticStatus_t::Truce:
        return DiplomaticStatus_t::Treaty;
    case DiplomaticStatus_t::Treaty:
        return DiplomaticStatus_t::Pact;
    case DiplomaticStatus_t::Vendetta:
        return DiplomaticStatus_t::Truce;
    case DiplomaticStatus_t::Pact:
        return std::nullopt;
    }
    return std::nullopt;
}

// The status a pair falls to when the agreement is canceled, broken, or expires.
constexpr std::optional<DiplomaticStatus_t> StepDown(DiplomaticStatus_t status)
{
    switch (status)
    {
    case DiplomaticStatus_t::Truce:
    case DiplomaticStatus_t::Treaty:
        return DiplomaticStatus_t::Neutral;
    case DiplomaticStatus_t::Pact:
        return DiplomaticStatus_t::Treaty;
    case DiplomaticStatus_t::Neutral:
    case DiplomaticStatus_t::Vendetta:
        return std::nullopt;
    }
    return std::nullopt;
}

// Empty string for Neutral; otherwise the status name.
std::string ToString(DiplomaticStatus_t status);

} // namespace ac
