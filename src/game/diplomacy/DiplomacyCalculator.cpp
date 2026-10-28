#include "game/diplomacy/DiplomacyCalculator.h"

#include "game/diplomacy/DiplomacyConfig.h"

#include <algorithm>

namespace ac
{

DiplomacyCalculator::DiplomacyCalculator(const DiplomacyConfig_t& rConfig)
    : m_rConfig(rConfig)
{}

int DiplomacyCalculator::ClampFriction(int friction) const
{
    return std::clamp(friction, 0, m_rConfig.frictionMax);
}

} // namespace ac
