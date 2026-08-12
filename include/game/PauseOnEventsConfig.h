#pragma once

namespace ac
{

// Mid-turn pause reasons the player can disable. InteractionPresenter Completes without UI
// when the matching flag is false so the turn does not stall. Prototype and Transcendence
// delay are stored for the settings UI; nothing enqueues them yet.
enum class PauseOnEventId_t
{
    NewFacilityBuilt,
    NonCombatUnitBuilt,
    CombatUnitBuilt,
    PrototypeBuilt,
    DroneRiots,
    EndOfDroneRiots,
    GoldenAgeStarts,
    EndOfGoldenAge,
    NutrientLow,
    BuildOrdersOutOfDate,
    PopulationLimitReached,
    DelayInTranscendence,
};

struct PauseOnEventsConfig_t
{
    bool newFacilityBuilt = true;
    bool nonCombatUnitBuilt = true;
    bool combatUnitBuilt = true;
    bool prototypeBuilt = true;
    bool droneRiots = true;
    bool endOfDroneRiots = true;
    bool goldenAgeStarts = true;
    bool endOfGoldenAge = true;
    bool nutrientLow = true;
    bool buildOrdersOutOfDate = true;
    bool populationLimitReached = true;
    bool delayInTranscendence = true;

    bool operator==(const PauseOnEventsConfig_t&) const = default;

    bool Allows(PauseOnEventId_t event) const
    {
        switch (event)
        {
        case PauseOnEventId_t::NewFacilityBuilt:
            return newFacilityBuilt;
        case PauseOnEventId_t::NonCombatUnitBuilt:
            return nonCombatUnitBuilt;
        case PauseOnEventId_t::CombatUnitBuilt:
            return combatUnitBuilt;
        case PauseOnEventId_t::PrototypeBuilt:
            return prototypeBuilt;
        case PauseOnEventId_t::DroneRiots:
            return droneRiots;
        case PauseOnEventId_t::EndOfDroneRiots:
            return endOfDroneRiots;
        case PauseOnEventId_t::GoldenAgeStarts:
            return goldenAgeStarts;
        case PauseOnEventId_t::EndOfGoldenAge:
            return endOfGoldenAge;
        case PauseOnEventId_t::NutrientLow:
            return nutrientLow;
        case PauseOnEventId_t::BuildOrdersOutOfDate:
            return buildOrdersOutOfDate;
        case PauseOnEventId_t::PopulationLimitReached:
            return populationLimitReached;
        case PauseOnEventId_t::DelayInTranscendence:
            return delayInTranscendence;
        }
        return true;
    }
};

} // namespace ac
