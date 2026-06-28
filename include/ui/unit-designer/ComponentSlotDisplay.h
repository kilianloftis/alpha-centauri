#pragma once

#include "ui/UIElement.h"
#include <functional>
#include <string>

namespace ac
{

struct UnitComponentConfig_t;

class ComponentSlotDisplay : public UIElement
{
public:
    ComponentSlotDisplay(
        const std::string& rLabel,
        std::function<const UnitComponentConfig_t*()> getComponent,
        WindowLayout_t layout,
        std::function<void()> onClicked = nullptr
    );
    ~ComponentSlotDisplay() override = default;

    void Render(Graphics& rGraphics) override;
    void HandleMouseClick(const MouseEvent_t& rEvent) override;

private:
    std::string m_label;
    std::function<const UnitComponentConfig_t*()> m_getComponent;
    std::function<void()> m_onClicked;

    static constexpr float k_LabelFontSizeRatio = 0.08f;
    static constexpr float k_NameFontSizeRatio  = 0.07f;
    static constexpr float k_PaddingRatio        = 0.04f;
};

} // namespace ac
