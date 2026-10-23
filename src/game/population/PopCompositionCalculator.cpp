#include "game/population/PopCompositionCalculator.h"
#include "game/population/PopTypeRegistry.h"

#include <algorithm>
#include <stdexcept>

namespace ac
{

PopCompositionCalculator::PopCompositionCalculator(const PopCompositionConfig_t& rConfig,
                                                   const PopTypeRegistry& rPopTypes)
    : m_rConfig(rConfig)
    , m_rPopTypes(rPopTypes)
{
    for (const PopTypeConfig_t& rType : m_rPopTypes.GetAll())
    {
        if (rType.droneWeight > 0)
        {
            m_droneTiers.push_back({rType.id, rType.droneWeight, rType.psychToPromote});
        }
        if (rType.bIsDefault)
        {
            m_workerPsychToPromote = rType.psychToPromote;
        }
    }
    std::sort(m_droneTiers.begin(), m_droneTiers.end(),
              [](const DroneTier_t& rLeft, const DroneTier_t& rRight) {
                  return rLeft.weight < rRight.weight;
              });

    if (m_droneTiers.empty())
    {
        throw std::runtime_error(
            "Pop composition: no pop type declares a positive drone_weight, so drone pressure "
            "could never be seated");
    }
}

const PopCompositionConfig_t& PopCompositionCalculator::GetConfig() const
{
    return m_rConfig;
}

// Every body at the lightest tier, then the cheapest upgrades, so lighter types win ({1,2,3},
// 3 bodies, 5 pressure gives {2,2,1}). Never overshoots; a remainder too small to upgrade drops.
std::vector<DroneSeat_t> PopCompositionCalculator::SeatPressure_(int pressure, int bodies,
                                                                 int& rDropped) const
{
    const int lightest = m_droneTiers.front().weight;
    std::vector<int> counts(m_droneTiers.size(), 0);

    const int seatedBodies = std::min(bodies, pressure / lightest);
    counts[0] = seatedBodies;
    int seated = seatedBodies * lightest;

    while (seated < pressure)
    {
        int bestTier = -1;
        int bestStep = 0;
        for (size_t tier = 0; tier + 1 < m_droneTiers.size(); ++tier)
        {
            if (counts[tier] == 0)
            {
                continue;
            }
            const int step = m_droneTiers[tier + 1].weight - m_droneTiers[tier].weight;
            if (seated + step > pressure)
            {
                continue;
            }
            if (bestTier < 0 || step < bestStep)
            {
                bestTier = static_cast<int>(tier);
                bestStep = step;
            }
        }
        if (bestTier < 0)
        {
            break;
        }
        --counts[bestTier];
        ++counts[bestTier + 1];
        seated += bestStep;
    }

    rDropped = pressure - seated;

    std::vector<DroneSeat_t> seats;
    for (size_t tier = 0; tier < m_droneTiers.size(); ++tier)
    {
        if (counts[tier] > 0)
        {
            seats.push_back({m_droneTiers[tier].typeId, counts[tier]});
        }
    }
    return seats;
}

// The psych_to_promote of the heaviest seat this pressure produces; 0 when nothing is seated.
int PopCompositionCalculator::PsychToEaseWorstDrone_(int pressure, int pool) const
{
    int ignoredDropped = 0;
    const std::vector<DroneSeat_t> seats = SeatPressure_(pressure, pool, ignoredDropped);
    if (seats.empty())
    {
        return 0;
    }
    const std::string& rWorstId = seats.back().typeId;
    const auto tier = std::ranges::find(m_droneTiers, rWorstId, &DroneTier_t::typeId);
    return tier->psychToPromote;
}

PopCompositionResult_t
PopCompositionCalculator::Calculate(const PopCompositionInputs_t& rInputs) const
{
    PopCompositionResult_t result;

    int pressure = std::max(0, rInputs.dronePressure);
    int psych = std::max(0, rInputs.psychAvailable);
    const int pool = std::max(0, rInputs.poolSize);

    // Worst first: each step moves one body down a tier for one pressure, at that type's
    // psych_to_promote.
    while (pressure > 0)
    {
        const int cost = PsychToEaseWorstDrone_(pressure, pool);
        if (cost <= 0 || psych < cost)
        {
            break;
        }
        psych -= cost;
        --pressure;
    }

    int talents = std::max(0, rInputs.resolvedTalents);

    // With no pressure left, psych promotes plain workers to talents at the default type's cost,
    // within the bodies left.
    while (m_workerPsychToPromote > 0 && psych >= m_workerPsychToPromote && pressure == 0
           && talents < pool)
    {
        psych -= m_workerPsychToPromote;
        ++talents;
    }

    result.psychSpent = std::max(0, rInputs.psychAvailable) - psych;

    // Annihilate: a talent cancels a drone when the two together do not fit. Each cancelled
    // pair removes one unit of pressure and one talent, so the overflow closes two at a time.
    const int overflow = pressure + talents - pool;
    if (overflow > 0 && talents > 0)
    {
        const int cancelled = std::min(talents, (overflow + 1) / 2);
        talents -= cancelled;
        pressure = std::max(0, pressure - cancelled);
    }

    talents = std::min(talents, pool);
    result.expectedTalents = talents;
    result.droneSeats = SeatPressure_(pressure, pool - talents, result.droppedPressure);
    return result;
}

} // namespace ac
