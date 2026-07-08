#pragma once

#include "game/faction/FactionConfig.h"

namespace ac
{

class AIProfile
{
public:
    AIProfile();
    explicit AIProfile(const AITendenciesConfig& rConfig);
    ~AIProfile();

    bool InterestedInWealth() const { return m_tendencies.wealth; }
    bool InterestedInPower() const { return m_tendencies.power; }
    bool InterestedInGrowth() const { return m_tendencies.growth; }
    bool InterestedInTech() const { return m_tendencies.tech; }

private:
    AITendenciesConfig m_tendencies;
};

} // namespace ac
