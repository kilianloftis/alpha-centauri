#pragma once

#include "ui/base/IBasePanel.h"
#include "lib/EventBus.h"
#include <string>

namespace ac
{

class Graphics;
class PopContainer;

// Displays current population and updates when population changes
class PopulationDisplay : public IBasePanel
{
public:
    PopulationDisplay(EventBus& rBus, Graphics& rGraphics);
    ~PopulationDisplay();

    // Set the screen position for IBasePanel rendering
    void SetRenderPosition(float x, float y);

    // Set the population directly (for initialization)
    void SetCurrentPop(int pop);

    // Provide a population container to display per-pop type breakdown
    void SetPopulation(const PopContainer* pPopContainer);

    // Get current displayed population
    int GetCurrentPop() const;

    // IBasePanel: renders at the stored position
    void Render(Graphics& rGraphics) override;

    // Render the population display at specified position
    void Render(float x, float y);

private:
    EventBus& m_rBus;
    Graphics& m_rGraphics;
    int m_currentPop;
    const PopContainer* m_pPopulation = nullptr;
    SubscriptionId m_subscriptionId;
    float m_renderX = 620.f;
    float m_renderY = 40.f;

    void OnPopGained_(const EvBaseGainedPop& event);
};

} // namespace ac
