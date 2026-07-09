#pragma once

#include "ui/IGameView.h"
#include "ui/UIElement.h"
#include <functional>

namespace ac
{

class Graphics;
class PopulationManager;
class Pop;

using PopClickCallback_t = std::function<void(Pop&)>;

// Displays current population as clickable buttons
class PopulationDisplay : public UIElement
{
public:
    PopulationDisplay(PopulationManager* pPopulation, WindowLayout_t layout, PopClickCallback_t onPopClick);
    ~PopulationDisplay() override = default;

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    struct PopBox_t
    {
        WindowLayout_t bounds;
        Pop* pPop;
    };

    PopulationManager* m_pPopulation = nullptr;
    PopClickCallback_t m_onPopClick;
    std::vector<PopBox_t> m_popBoxes;
};

} // namespace ac
