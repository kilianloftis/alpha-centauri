#pragma once

#include "ui/UIGroup.h"
#include "ui/base/PopButton.h"

namespace ac
{

class Graphics;
class PopContainer;

// Displays current population as clickable buttons
class PopulationDisplay : public UIGroup
{
public:
    PopulationDisplay(const Graphics& rGraphics, const PopContainer* pPopContainer, ResolvedLayout_t layout, PopButton::OnClickCallback_t onPopClick);
    ~PopulationDisplay() override = default;

    void Update() override;
    void Render() override;
    void HandleKey(const KeyEvent_t& rEvent) override;

private:
    const PopContainer* m_pPopulation = nullptr;
    PopButton::OnClickCallback_t m_onPopClick;

    static constexpr float kHeaderFontSizeRatio = 0.04f;
    static constexpr float kEntryFontSizeRatio  = 0.03f;
    static constexpr float kLineHeightRatio     = 0.05f;
};

} // namespace ac
