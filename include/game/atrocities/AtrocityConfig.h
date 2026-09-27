#pragma once

#include <magic_enum.hpp>

#include <array>
#include <cstddef>
#include <string_view>

namespace ac
{

// The two forbidden-act tiers the Datalinks name. Closed: a config file configures these,
// it does not add new ones.
enum class AtrocitySeverityId_t
{
    Simple,
    Major,
};

inline constexpr std::size_t k_AtrocitySeverityCount =
    magic_enum::enum_count<AtrocitySeverityId_t>();

// Display spelling, kept beside the enum so no call site re-derives one from the enumerator
// name. Throws when id is not one of the closed set.
std::string_view AtrocitySeverityLabel(AtrocitySeverityId_t id);

// What one severity costs its perpetrator. Every field is required in config/atrocities.json:
// these are game rules per tier, and a C++ default here would silently stand in for a key a
// modder meant to set.
struct AtrocitySeverityConfig_t
{
    // Added to the eco-damage mineral term once per counted record.
    int ecoVirtualMinerals = 0;
    bool bUniversalVendetta = false;
    bool bExpelFromCouncil = false;
};

struct AtrocitiesConfig_t
{
    std::array<AtrocitySeverityConfig_t, k_AtrocitySeverityCount> severities{};

    // A counted Simple act adds this many years times the simple-atrocity count after that
    // act, onto whatever sanction time is left. Major acts, including an escalated Simple
    // act, add none.
    int sanctionYearsPerAtrocity = 0;

    // Throws when id is not one of the closed set. Callers reach this only with a value the
    // parser has already accepted.
    const AtrocitySeverityConfig_t& For(AtrocitySeverityId_t id) const;
};

} // namespace ac
