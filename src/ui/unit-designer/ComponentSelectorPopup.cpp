#include "ui/unit-designer/ComponentSelectorPopup.h"
#include "game/units/UnitComponentConfig.h"
#include "graphics/Graphics.h"
#include "input/Input.h"

namespace ac
{

namespace
{

constexpr Color_t k_BackgroundColor       {20, 20, 40, 255};
constexpr Color_t k_BorderColor           {100, 100, 180, 255};
constexpr float k_BorderWidth           = 2.0f;
constexpr float k_TitleFontSizeRatio    = 0.05f;
constexpr float k_EntryFontSizeRatio    = 0.04f;
constexpr float k_EntryHeightRatio      = 0.07f;
constexpr float k_PaddingRatio          = 0.03f;
constexpr float k_TitleHeightMultiplier = 2.0f;

} // namespace

ComponentSelectorPopup::ComponentSelectorPopup(
    std::vector<const UnitComponentConfig_t*> components,
    WindowLayout_t layout,
    std::function<void(const UnitComponentConfig_t&)> onSelected
)
    : UIElement(layout)
    , m_components(std::move(components))
    , m_onSelected(std::move(onSelected))
{
    CacheEntryRects_();
}

void ComponentSelectorPopup::CacheEntryRects_()
{
    const float padding     = m_layout.height * k_PaddingRatio;
    const float entryHeight = m_layout.height * k_EntryHeightRatio;
    const float titleHeight = m_layout.height * k_TitleFontSizeRatio * k_TitleHeightMultiplier;

    m_entryRects.clear();
    float y = m_layout.y + padding + titleHeight;
    for (size_t i = 0; i < m_components.size(); ++i)
    {
        m_entryRects.push_back({m_layout.x, y, m_layout.width, entryHeight});
        y += entryHeight;
    }
}

void ComponentSelectorPopup::Render(Graphics& rGraphics)
{
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BackgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, k_BorderColor, k_BorderWidth);

    const float padding          = m_layout.height * k_PaddingRatio;
    const unsigned int titleSize = static_cast<unsigned int>(m_layout.height * k_TitleFontSizeRatio);
    const unsigned int entrySize = static_cast<unsigned int>(m_layout.height * k_EntryFontSizeRatio);

    rGraphics.DrawText("Select Component", m_layout.x + padding, m_layout.y + padding, titleSize, Color_t::Yellow());

    for (size_t i = 0; i < m_components.size() && i < m_entryRects.size(); ++i)
    {
        const Rectangle_t& rRect = m_entryRects[i];
        rGraphics.DrawText(m_components[i]->name, rRect.x + padding, rRect.y, entrySize);
    }
}

void ComponentSelectorPopup::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (rEvent.button != MouseButton_t::Left)
    {
        return;
    }

    for (size_t i = 0; i < m_entryRects.size(); ++i)
    {
        if (ContainsMouseCoord(m_entryRects[i], rEvent))
        {
            if (m_onSelected)
            {
                m_onSelected(*m_components[i]);
            }
            m_bShouldClose = true;
            return;
        }
    }
}

bool ComponentSelectorPopup::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
        return true;
    }
    return false;
}

} // namespace ac
