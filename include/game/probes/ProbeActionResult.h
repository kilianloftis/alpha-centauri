#pragma once

#include "game/buildings/BuildingConfig.h"
#include "lib/GameEvent.h"

#include <string>
#include <variant>

namespace ac
{

enum class ProbeActionOutcome_t
{
    Rejected,       // ineligible, cannot pay, missing config
    MissionFailed,  // probe destroyed on mission failure
    Captured,       // mission succeeded but escape failed (probe destroyed)
    Succeeded,      // mission + escape
};

struct ProbeChance_t
{
    // 0..100 style rates matching Thinker display. The integer mission/escape rolls use
    // probeStrength / escapeStrength and the save and bust chances, not these percents.
    int successRate = 100;
    int survivalRate = 100;
    int risk = 0;
    int probeStrength = 1;
    int escapeStrength = 1;
    // Percent chance a failed mission or escape draw is saved.
    int failureSaveChance = 0;
    // Percent chance a successful mission or escape draw is busted anyway.
    int successBustChance = 0;
};

enum class ProbeFrameOutcome_t
{
    // The victim blames the framed faction, and its keep-or-break toward the actor is skipped.
    Framed,
    // The frame failed: the framed faction turns on the actor.
    Flopped,
};

struct ProbeInfiltrated_t
{
};

// The probe died on contact with the target's hunter. hunterSourceId is the building or project
// that contributes the most probe_hunter.
struct ProbeContactDestroyed_t
{
    BuildingId_t hunterSourceId;
};

struct ProbeNoTech_t
{
};

struct ProbeStolenMap_t
{
};

struct ProbeRiot_t
{
};

struct ProbeIncreasedActivity_t
{
};

struct ProbeBaseCaptured_t
{
};

struct ProbeUnitSubverted_t
{
};

// Targeted sabotage named a facility the base does not have (or named none). Distinct from
// the random-sabotage lane, which falls back to wiping production.
struct ProbeNoTarget_t
{
};

struct ProbeStolenTech_t
{
    TechId techId;
};

struct ProbeDestroyedFacility_t
{
    BuildingId_t buildingId;
};

struct ProbeEnergyStolen_t
{
    int amount = 0;
};

struct ProbePopulationKilled_t
{
    int count = 0;
};

struct ProbeProductionWiped_t
{
    std::string itemName;
};

struct ProbeResearchWiped_t
{
    int points = 0;
};

using ProbeActionDetail_t = std::variant<
    std::monostate,
    ProbeInfiltrated_t,
    ProbeContactDestroyed_t,
    ProbeNoTech_t,
    ProbeStolenMap_t,
    ProbeRiot_t,
    ProbeIncreasedActivity_t,
    ProbeBaseCaptured_t,
    ProbeUnitSubverted_t,
    ProbeNoTarget_t,
    ProbeStolenTech_t,
    ProbeDestroyedFacility_t,
    ProbeEnergyStolen_t,
    ProbePopulationKilled_t,
    ProbeProductionWiped_t,
    ProbeResearchWiped_t>;

struct ProbeActionResult_t
{
    ProbeActionOutcome_t outcome = ProbeActionOutcome_t::Rejected;
    ProbeChance_t chances;
    ProbeActionDetail_t detail;
};

} // namespace ac
