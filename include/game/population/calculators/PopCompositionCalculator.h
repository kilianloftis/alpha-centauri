#pragma once

#include "game/population/pop-types/PopCompositionConfigParser.h"
#include <string>
#include <vector>

namespace ac
{

class PopTypeRegistry;

struct PopCompositionInputs_t
{
    // Finalize(StatId_t::Drones): drone *pressure*, summed from every source and clamped.
    // Not a headcount — a Super Drone body absorbs two of it.
    int dronePressure = 0;
    // Finalize(StatId_t::Talents). Seats with no psych cost; the ladder is the only psych path.
    int resolvedTalents = 0;
    // This turn's psych. Read, never consumed — see ResourceManager::GetPsych.
    int psychAvailable = 0;
    // Non-specialist pop count: the bodies available to carry pressure and talents.
    int poolSize = 0;
};

// How many bodies of one drone-class type to seat, ascending by drone_weight.
struct DroneSeat_t
{
    std::string typeId;
    int count = 0;
};

struct PopCompositionResult_t
{
    std::vector<DroneSeat_t> droneSeats;
    int expectedTalents = 0;
    // Pressure that did not fit even with every body at the heaviest tier. Discarded: a base
    // with any super drone is already rioting, so the remainder cannot change the outcome.
    int droppedPressure = 0;
    int psychSpent = 0;
};

// Phase 1 of composition: turns drone pressure, talents and psych into the counts a base
// should have. Works entirely on counts — no pop instance is touched here, which is why it can
// run before phase 2 exists.
//
// Order: psych ladder, then annihilate drone/talent pairs that do not fit, then seat the
// remaining pressure lightest-type-first.
class PopCompositionCalculator
{
public:
    PopCompositionCalculator(const PopCompositionConfig_t& rConfig,
                             const PopTypeRegistry& rPopTypes);
    ~PopCompositionCalculator() = default;

    PopCompositionResult_t Calculate(const PopCompositionInputs_t& rInputs) const;
    const PopCompositionConfig_t& GetConfig() const;

private:
    // Drone-class types ascending by drone_weight, cached at construction.
    struct DroneTier_t
    {
        std::string typeId;
        int weight = 0;
        int psychToPromote = 0;
    };

    std::vector<DroneSeat_t> SeatPressure_(int pressure, int bodies, int& rDropped) const;

    const PopCompositionConfig_t& m_rConfig;
    const PopTypeRegistry& m_rPopTypes;
    std::vector<DroneTier_t> m_droneTiers;
    int m_workerPsychToPromote = 0;
};

} // namespace ac
