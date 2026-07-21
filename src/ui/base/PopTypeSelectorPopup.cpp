#include "ui/base/PopTypeSelectorPopup.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "graphics/Graphics.h"
#include "input/Input.h"
#include "ui/style/UiStyle.h"

namespace ac
{

PopTypeSelectorPopup::PopTypeSelectorPopup(
    std::vector<const PopTypeConfig_t*> popTypes,
    WindowLayout_t layout,
    std::function<void(const PopTypeConfig_t&)> onPopTypeSelected
)
    : UIElement(layout)
    , m_popTypes(std::move(popTypes))
    , m_onPopTypeSelected(std::move(onPopTypeSelected))
{
    CacheEntryRects_();
}

void PopTypeSelectorPopup::CacheEntryRects_()
{
    const auto& style = Style().popTypeSelectorPopup;
    const float lineHeight = m_layout.height * style.lineHeightRatio;

    float offsetY = lineHeight * style.headerLineOffset;
    for (size_t i = 0; i < m_popTypes.size(); ++i)
    {
        m_entryRects.push_back(Rectangle_t{
            m_layout.x,
            m_layout.y + offsetY,
            m_layout.width,
            lineHeight
        });
        offsetY += lineHeight;
    }
}

void PopTypeSelectorPopup::Render(Graphics& rGraphics)
{
    if (m_bShouldClose)
    {
        return;
    }

    const auto& style = Style().popTypeSelectorPopup;
    const float padding = style.paddingRatio * m_layout.width;

    const unsigned int headerFontSize = static_cast<unsigned int>(m_layout.height * style.headerFontSizeRatio);
    const unsigned int entryFontSize  = static_cast<unsigned int>(m_layout.height * style.entryFontSizeRatio);
    const float        lineHeight     = m_layout.height * style.lineHeightRatio;

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.borderColor);

    rGraphics.DrawText("Select Pop Type", m_layout.x + padding, m_layout.y + padding, headerFontSize, style.headerColor);

    if (m_popTypes.empty())
    {
        rGraphics.DrawText(
            "No pop types available",
            m_layout.x + padding,
            m_layout.y + lineHeight * style.headerLineOffset,
            entryFontSize,
            style.hintColor
        );
        return;
    }

    for (size_t i = 0; i < m_popTypes.size(); ++i)
    {
        const Rectangle_t& rect = m_entryRects[i];
        rGraphics.DrawText(m_popTypes[i]->name, rect.x + padding, rect.y, entryFontSize, style.entryColor);
    }
}

bool PopTypeSelectorPopup::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
        return true;
    }
    return false;
}

void PopTypeSelectorPopup::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (m_bShouldClose || rEvent.button != MouseButton_t::Left)
    {
        return;
    }

    if (!ContainsMouseCoord(m_layout, rEvent))
    {
        return;
    }

    for (size_t i = 0; i < m_entryRects.size(); ++i)
    {
        const Rectangle_t& rect = m_entryRects[i];
        if (ContainsMouseCoord(rect, rEvent))
        {
            if (m_onPopTypeSelected)
            {
                m_onPopTypeSelected(*m_popTypes[i]);
                m_bShouldClose = true;
            }
            return;
        }
    }
}

} // namespace ac
