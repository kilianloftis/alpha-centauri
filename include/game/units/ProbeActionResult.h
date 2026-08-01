#pragma once

#include "game/buildings/BuildingConfigParser.h"
#include "game/research/TechConfigParser.h"
#include "game/units/ProbeRules.h"

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

// Status-only outcomes with no payload.
enum class ProbeActionStatus_t
{
    Infiltrated,
    NoTech,
    ProductionWiped,
    Riot,
    ResearchWiped,
    BaseCaptured,
    UnitSubverted,
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

using ProbeActionDetail_t = std::variant<
    std::monostate,
    ProbeActionStatus_t,
    ProbeStolenTech_t,
    ProbeDestroyedFacility_t,
    ProbeEnergyStolen_t,
    ProbePopulationKilled_t>;

struct ProbeActionResult_t
{
    ProbeActionOutcome_t outcome = ProbeActionOutcome_t::Rejected;
    ProbeChance_t chances;
    ProbeActionDetail_t detail;
};

} // namespace ac
