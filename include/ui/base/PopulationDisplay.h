#pragma once

#include "ui/IGameView.h"
#include "ui/UIElement.h"
#include <functional>

namespace ac
{

class Graphics;
class PopContainer;
class Pop;

using PopClickCallback_t = std::function<void(Pop&)>;

// Displays current population as clickable buttons
class PopulationDisplay : public UIElement
{
public:
    PopulationDisplay(const PopContainer* pPopContainer, WindowLayout_t layout, PopClickCallback_t onPopClick);
    ~PopulationDisplay() override = default;

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    struct PopBox_t
    {
        WindowLayout_t bounds;
        Pop* pPop;
    };

    const PopContainer* m_pPopulation = nullptr;
    PopClickCallback_t m_onPopClick;
    std::vector<PopBox_t> m_popBoxes;
};

} // namespace ac
