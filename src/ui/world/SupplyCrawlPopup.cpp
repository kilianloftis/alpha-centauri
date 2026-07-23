#include "ui/world/SupplyCrawlPopup.h"
#include "graphics/Graphics.h"
#include "input/Input.h"
#include "ui/style/UiStyle.h"

namespace ac
{

SupplyCrawlPopup::SupplyCrawlPopup(
    WindowLayout_t layout,
    std::function<void(StatId_t)> onResourceSelected
)
    : UIElement(layout)
    , m_onResourceSelected(std::move(onResourceSelected))
{
    m_entries = {
        {StatId_t::Nutrients, "Nutrients"},
        {StatId_t::Minerals, "Minerals"},
        {StatId_t::Energy, "Energy"},
    };
    CacheEntryRects_();
}

void SupplyCrawlPopup::CacheEntryRects_()
{
    const auto& style = Style().productionSelectorPopup;
    const float lineHeight = m_layout.height * style.lineHeightRatio;
    float offsetY = lineHeight * style.headerLineOffset;
    for (size_t i = 0; i < m_entries.size(); ++i)
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

void SupplyCrawlPopup::Render(Graphics& rGraphics)
{
    if (m_bShouldClose)
    {
        return;
    }

    const auto& style = Style().productionSelectorPopup;
    const float padding = style.paddingRatio * m_layout.width;
    const unsigned int headerFontSize = static_cast<unsigned int>(m_layout.height * style.headerFontSizeRatio);
    const unsigned int entryFontSize  = static_cast<unsigned int>(m_layout.height * style.entryFontSizeRatio);

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.borderColor);

    rGraphics.DrawText("Supply Crawl", m_layout.x + padding, m_layout.y + padding, headerFontSize, style.headerColor);

    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        const Rectangle_t& rect = m_entryRects[i];
        rGraphics.DrawText(m_entries[i].label, rect.x + padding, rect.y, entryFontSize, style.entryColor);
    }
}

bool SupplyCrawlPopup::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
        return true;
    }
    return false;
}

void SupplyCrawlPopup::HandleMouseClick(const MouseEvent_t& rEvent)
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
            if (m_onResourceSelected)
            {
                m_onResourceSelected(m_entries[i].resource);
            }
            m_bShouldClose = true;
            return;
        }
    }
}

} // namespace ac
