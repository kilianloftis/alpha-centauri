#pragma once

#include "lib/EventBus.h"
#include <string>

namespace ac
{

class Graphics;
class PopulationManager;

// Displays current population and updates when population changes
class PopulationDisplay
{
public:
    PopulationDisplay(EventBus& rBus, Graphics& rGraphics);
    ~PopulationDisplay();

    // Render the population display at specified position
    void Render(float x, float y);

    // Get current displayed population
    int GetCurrentPop() const;

    // Set the population directly (for initialization)
    void SetCurrentPop(int pop);

    // Provide a population to display per-pop type breakdown
    void SetPopulation(const PopulationManager* pPopulation);

private:
    EventBus& m_rBus;
    Graphics& m_rGraphics;
    int m_currentPop;
    const PopulationManager* m_pPopulation = nullptr;
    SubscriptionId m_subscriptionId;

    void OnPopGained_(const EvBaseGainedPop& event);
};

} // namespace ac
