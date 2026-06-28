#include "ui/unit-designer/ComponentSelectorPopup.h"
#include "game/units/UnitComponentConfig.h"
#include "graphics/Graphics.h"
#include "input/Input.h"

namespace ac
{

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
    const float titleHeight = m_layout.height * k_TitleFontSizeRatio * 2.0f;

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
    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, Color{20, 20, 40, 255});
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, Color{100, 100, 180, 255}, 2.0f);

    const float padding          = m_layout.height * k_PaddingRatio;
    const unsigned int titleSize = static_cast<unsigned int>(m_layout.height * k_TitleFontSizeRatio);
    const unsigned int entrySize = static_cast<unsigned int>(m_layout.height * k_EntryFontSizeRatio);

    rGraphics.DrawText("Select Component", m_layout.x + padding, m_layout.y + padding, titleSize, Color::Yellow());

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
