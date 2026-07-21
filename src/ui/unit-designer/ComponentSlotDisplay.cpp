#include "ui/unit-designer/ComponentSlotDisplay.h"
#include "game/units/UnitComponentConfig.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

namespace ac
{

ComponentSlotDisplay::ComponentSlotDisplay(
    const std::string& rLabel,
    std::function<const UnitComponentConfig_t*()> getComponent,
    WindowLayout_t layout,
    std::function<void()> onClicked
)
    : UIElement(layout)
    , m_label(rLabel)
    , m_getComponent(std::move(getComponent))
    , m_onClicked(std::move(onClicked))
{}

void ComponentSlotDisplay::Render(Graphics& rGraphics)
{
    rGraphics.DrawFilledRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        Style().componentSlotDisplay.backgroundColor
    );
    rGraphics.DrawRect(
        m_layout.x, m_layout.y, m_layout.width, m_layout.height,
        Style().componentSlotDisplay.borderColor
    );

    const float padding = m_layout.height * Style().componentSlotDisplay.paddingRatio;
    const unsigned int labelSize = static_cast<unsigned int>(
        m_layout.height * Style().componentSlotDisplay.labelFontSizeRatio);
    const unsigned int nameSize = static_cast<unsigned int>(
        m_layout.height * Style().componentSlotDisplay.nameFontSizeRatio);

    rGraphics.DrawText(
        m_label, m_layout.x + padding, m_layout.y + padding, labelSize,
        Style().componentSlotDisplay.labelTextColor
    );

    const UnitComponentConfig_t* pComponent = m_getComponent();
    const std::string& rName = pComponent ? pComponent->name : "(none)";
    const Color_t nameColor = pComponent
        ? Style().componentSlotDisplay.filledNameColor
        : Style().componentSlotDisplay.emptyNameColor;
    rGraphics.DrawText(
        rName,
        m_layout.x + padding,
        m_layout.y + padding + static_cast<float>(labelSize)
            * Style().componentSlotDisplay.nameLabelSpacingMultiplier,
        nameSize,
        nameColor
    );
}

void ComponentSlotDisplay::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (rEvent.button == MouseButton_t::Left && m_onClicked)
    {
        m_onClicked();
    }
}

} // namespace ac
