#include "ui/unit-designer/ComponentSlotDisplay.h"
#include "game/units/UnitComponentConfig.h"
#include "graphics/Graphics.h"

namespace ac
{

namespace
{

constexpr Color k_BackgroundColor              {30, 30, 35, 255};
constexpr Color k_BorderColor                  {80, 80, 100, 255};
constexpr Color k_LabelTextColor               {150, 150, 170, 255};
constexpr Color k_EmptyNameColor               {80, 80, 80, 255};
constexpr float k_LabelFontSizeRatio           = 0.08f;
constexpr float k_NameFontSizeRatio            = 0.07f;
constexpr float k_PaddingRatio                 = 0.04f;
constexpr float k_NameLabelSpacingMultiplier   = 1.4f;

} // namespace

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
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BackgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BorderColor);

    const float padding          = m_layout.height * k_PaddingRatio;
    const unsigned int labelSize = static_cast<unsigned int>(m_layout.height * k_LabelFontSizeRatio);
    const unsigned int nameSize  = static_cast<unsigned int>(m_layout.height * k_NameFontSizeRatio);

    rGraphics.DrawText(m_label, m_layout.x + padding, m_layout.y + padding, labelSize, k_LabelTextColor);

    const UnitComponentConfig_t* pComponent = m_getComponent();
    const std::string& rName = pComponent ? pComponent->name : "(none)";
    const Color nameColor    = pComponent ? Color::White() : k_EmptyNameColor;
    rGraphics.DrawText(
        rName,
        m_layout.x + padding,
        m_layout.y + padding + static_cast<float>(labelSize) * k_NameLabelSpacingMultiplier,
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
