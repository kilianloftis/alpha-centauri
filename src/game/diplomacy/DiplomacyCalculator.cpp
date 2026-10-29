#include "game/diplomacy/DiplomacyCalculator.h"

#include "game/diplomacy/DiplomacyConfig.h"

#include <algorithm>

namespace ac
{

DiplomacyCalculator::DiplomacyCalculator(const DiplomacyConfig_t& rConfig)
    : m_rConfig(rConfig)
{}

int DiplomacyCalculator::NextFriction(int current, int delta) const
{
    return std::clamp(current + delta, 0, m_rConfig.frictionMax);
}

int DiplomacyCalculator::NextBlemishes(int current, int delta, int floor) const
{
    const int raised = std::max(current + delta, floor);
    return std::clamp(raised, 0, m_rConfig.blemishesMax);
}

} // namespace ac
