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

    static constexpr float k_HeaderFontSizeRatio    = 0.04f;
    static constexpr float k_EntryFontSizeRatio     = 0.03f;
    static constexpr float k_LineHeightRatio        = 0.05f;
    static constexpr float k_PopBoxSizeRatio        = 0.6f;
    static constexpr float k_PopBoxSpacingRatio     = 0.02f;
    static constexpr float k_LeftPaddingRatio       = 0.02f;
    static constexpr float k_PopBoxFontSizeRatio    = 0.6f;
    static constexpr float k_PopRowYOffsetRatio     = 0.02f;
    static constexpr float k_PopBoxTextXOffsetRatio = 0.35f;
    static constexpr float k_PopBoxTextYOffsetRatio = 0.2f;
};

} // namespace ac
