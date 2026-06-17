#pragma once

#include "ui/base/IBasePanel.h"
#include "lib/EventBus.h"
#include <string>

namespace ac
{

class Graphics;
class PopContainer;

// Displays current population and updates when population changes
class PopulationDisplay : public UIPanel
{
public:
    PopulationDisplay(const Graphics& rGraphics, const PopContainer* pPopContainer, PanelLayout_t layout);
    ~PopulationDisplay();


    // Render the population display at specified position
    void Render();

private:
    const Graphics& m_rGraphics;
    const PopContainer* m_pPopulation = nullptr;

    void OnPopGained_(const EvBaseGainedPop& event);
};

} // namespace ac
