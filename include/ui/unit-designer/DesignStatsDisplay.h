#pragma once

#include "ui/UIElement.h"
#include "ui/unit-designer/UnitDesignerState.h"
#include "game/units/UnitSlotConfig.h"
#include <functional>
#include <vector>

namespace ac
{

class DesignStatsDisplay : public UIElement
{
public:
    DesignStatsDisplay(
        const UnitDesignerState_t* pState,
        const std::vector<UnitSlotConfig_t>* pSlots,
        WindowLayout_t layout,
        std::function<void()> onSaveDesign = nullptr
    );
    ~DesignStatsDisplay() override = default;

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    const UnitDesignerState_t* m_pState;
    const std::vector<UnitSlotConfig_t>* m_pSlots;
    std::function<void()> m_onSaveDesign;
    Rectangle_t m_saveButtonRect{};

    static constexpr float k_HeaderFontSizeRatio   = 0.04f;
    static constexpr float k_StatFontSizeRatio     = 0.032f;
    static constexpr float k_LineHeightRatio        = 0.055f;
    static constexpr float k_PaddingRatio           = 0.02f;
    static constexpr float k_SaveButtonHeightRatio  = 0.07f;
};

} // namespace ac
