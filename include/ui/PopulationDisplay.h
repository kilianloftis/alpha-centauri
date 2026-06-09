#pragma once

#include "lib/EventBus.h"
#include <string>

namespace ac
{

class Graphics;

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

private:
    EventBus& m_rBus;
    Graphics& m_rGraphics;
    int m_currentPop;
    SubscriptionId m_subscriptionId;

    void OnPopGained_(const EvBaseGainedPop& event);
};

} // namespace ac
