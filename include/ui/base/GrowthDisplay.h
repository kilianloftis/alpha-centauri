#pragma once

#include "ui/UIElement.h"

namespace ac
{

class BaseManager;
class Graphics;
class GrowthCalculator;

class GrowthDisplay : public UIElement
{
public:
    GrowthDisplay(
        const BaseManager* pBase,
        const GrowthCalculator* pGrowthCalculator,
        WindowLayout_t layout
    );
    ~GrowthDisplay() override = default;

    void Render(Graphics& rGraphics) override;

private:
    const BaseManager* m_pBase = nullptr;
    const GrowthCalculator* m_pGrowthCalculator = nullptr;

    static constexpr float k_HeaderFontSizeRatio = 0.04f;
    static constexpr float k_EntryFontSizeRatio  = 0.03f;
    static constexpr float k_LineHeightRatio   = 0.05f;
    static constexpr float k_LeftPaddingRatio  = 0.02f;
};

} // namespace ac
