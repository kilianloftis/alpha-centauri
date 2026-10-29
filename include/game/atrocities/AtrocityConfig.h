#pragma once

#include <magic_enum.hpp>

#include <array>
#include <cstddef>

namespace ac
{

// The two forbidden-act tiers the Datalinks name. Closed: a config file configures these,
// it does not add new ones. Ordered mildest first; AtrocityLedger::HasVictimizedAtLeast compares.
enum class AtrocitySeverityId_t
{
    Simple,
    Major,
};

inline constexpr std::size_t k_AtrocitySeverityCount =
    magic_enum::enum_count<AtrocitySeverityId_t>();

// Why an act was not answered for. Decided once, so the notice does not re-derive it.
enum class AtrocityExcuse_t
{
    None,
    CharterRepealed,
    ProgenitorParty,
};

// What one severity costs; every field is required.
struct AtrocitySeverityConfig_t
{
    // Added to the eco-damage mineral term once per counted record.
    int ecoVirtualMinerals = 0;
    bool bUniversalVendetta = false;
    bool bExpelFromCouncil = false;
    // The victim's friction toward the perpetrator. Written only when the victim has diplomacy.
    int friction = 0;
    // Counted acts add this; a first act against a victim also raises to blemishesFloor.
    // Victimless uses the delta only (terranx-map.md, planet_busting).
    int blemishes = 0;
    int blemishesFloor = 0;
    int repeatBlemishes = 0;
};

struct AtrocitiesConfig_t
{
    std::array<AtrocitySeverityConfig_t, k_AtrocitySeverityCount> severities{};

    // A counted Simple act adds this times the Simple count after it to the sanction left; Major
    // acts add none.
    int sanctionYearsPerAtrocity = 0;

    // Throws when id is not one of the closed set. Callers reach this only with a value the
    // parser has already accepted.
    const AtrocitySeverityConfig_t& For(AtrocitySeverityId_t id) const;
};

} // namespace ac
