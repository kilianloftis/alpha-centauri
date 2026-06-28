#include "ui/unit-designer/ComponentSlotDisplay.h"
#include "game/units/UnitComponentConfig.h"
#include "graphics/Graphics.h"

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
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, Color{30, 30, 35, 255});
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, Color{80, 80, 100, 255});

    const float padding          = m_layout.height * k_PaddingRatio;
    const unsigned int labelSize = static_cast<unsigned int>(m_layout.height * k_LabelFontSizeRatio);
    const unsigned int nameSize  = static_cast<unsigned int>(m_layout.height * k_NameFontSizeRatio);

    rGraphics.DrawText(m_label, m_layout.x + padding, m_layout.y + padding, labelSize, Color{150, 150, 170, 255});

    const UnitComponentConfig_t* pComponent = m_getComponent();
    const std::string& rName = pComponent ? pComponent->name : "(none)";
    const Color nameColor    = pComponent ? Color::White() : Color{80, 80, 80, 255};
    rGraphics.DrawText(rName, m_layout.x + padding, m_layout.y + padding + static_cast<float>(labelSize) * 1.4f, nameSize, nameColor);
}

void ComponentSlotDisplay::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (rEvent.button == MouseButton_t::Left && m_onClicked)
    {
        m_onClicked();
    }
}

} // namespace ac
