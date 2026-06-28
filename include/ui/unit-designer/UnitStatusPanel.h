#pragma once

#include "ui/UIElement.h"
#include <functional>

namespace ac
{

class UnitDesign;
class UnitManager;

class UnitStatusPanel : public UIElement
{
public:
    UnitStatusPanel(
        std::function<const UnitDesign*()> getSelectedDesign,
        const UnitManager* pUnitManager,
        WindowLayout_t layout
    );
    ~UnitStatusPanel() override = default;

    void Render(Graphics& rGraphics) override;

private:
    std::function<const UnitDesign*()> m_getSelectedDesign;
    const UnitManager* m_pUnitManager;

    static constexpr float k_HeaderFontSizeRatio = 0.08f;
    static constexpr float k_StatFontSizeRatio   = 0.07f;
    static constexpr float k_LineHeightRatio      = 0.10f;
    static constexpr float k_PaddingRatio         = 0.04f;
};

} // namespace ac
