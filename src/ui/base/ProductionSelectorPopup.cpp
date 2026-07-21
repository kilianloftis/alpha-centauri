#include "ui/base/ProductionSelectorPopup.h"
#include "graphics/Graphics.h"
#include "input/Input.h"
#include "ui/style/UiStyle.h"

namespace ac
{

ProductionSelectorPopup::ProductionSelectorPopup(
    std::vector<const IConstructable*> availableItems,
    WindowLayout_t layout,
    std::function<void(const IConstructable&)> onItemSelected
)
    : UIElement(layout)
    , m_availableItems(std::move(availableItems))
    , m_onItemSelected(std::move(onItemSelected))
{
    CacheEntryRects_();
}

void ProductionSelectorPopup::CacheEntryRects_()
{
    const auto& style = Style().productionSelectorPopup;
    const float lineHeight = m_layout.height * style.lineHeightRatio;
    float offsetY = lineHeight * style.headerLineOffset;
    for (size_t i = 0; i < m_availableItems.size(); ++i)
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

void ProductionSelectorPopup::Render(Graphics& rGraphics)
{
    if (m_bShouldClose)
    {
        return;
    }

    const auto& style = Style().productionSelectorPopup;
    const float padding = style.paddingRatio * m_layout.width;
    const unsigned int headerFontSize = static_cast<unsigned int>(m_layout.height * style.headerFontSizeRatio);
    const unsigned int entryFontSize  = static_cast<unsigned int>(m_layout.height * style.entryFontSizeRatio);
    const float lineHeight = m_layout.height * style.lineHeightRatio;

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.borderColor);

    rGraphics.DrawText("Select Production", m_layout.x + padding, m_layout.y + padding, headerFontSize, style.headerColor);

    if (m_availableItems.empty())
    {
        rGraphics.DrawText(
            "Nothing available to build",
            m_layout.x + padding,
            m_layout.y + lineHeight * style.headerLineOffset,
            entryFontSize,
            style.hintColor
        );
        return;
    }

    for (size_t i = 0; i < m_availableItems.size(); ++i)
    {
        const Rectangle_t& rect = m_entryRects[i];
        rGraphics.DrawText(m_availableItems[i]->GetName(), rect.x + padding, rect.y, entryFontSize, style.entryColor);
    }
}

bool ProductionSelectorPopup::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
        return true;
    }
    return false;
}

void ProductionSelectorPopup::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (m_bShouldClose || rEvent.button != MouseButton_t::Left)
    {
        return;
    }

    if (!ContainsMouseCoord(m_layout, rEvent))
    {
        m_bShouldClose = true;
        return;
    }

    for (size_t i = 0; i < m_entryRects.size(); ++i)
    {
        if (ContainsMouseCoord(m_entryRects[i], rEvent))
        {
            if (m_onItemSelected && m_availableItems[i])
            {
                m_onItemSelected(*m_availableItems[i]);
                m_bShouldClose = true;
            }
            return;
        }
    }
}

} // namespace ac
