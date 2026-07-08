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

    float GetWealth() const { return m_tendencies.wealth; }
    float GetPower() const { return m_tendencies.power; }
    float GetGrowth() const { return m_tendencies.growth; }
    float GetTech() const { return m_tendencies.tech; }

private:
    AITendenciesConfig m_tendencies;
};

} // namespace ac
