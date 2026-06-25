#include "ui/base/ProductionSelectorPopup.h"
#include "graphics/Graphics.h"
#include "input/Input.h"

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
    const float lineHeight = m_layout.height * k_LineHeightRatio;
    float offsetY = lineHeight * 2.0f;
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

    const float padding = k_PaddingRatio * m_layout.width;
    const unsigned int headerFontSize = static_cast<unsigned int>(m_layout.height * k_HeaderFontSizeRatio);
    const unsigned int entryFontSize  = static_cast<unsigned int>(m_layout.height * k_EntryFontSizeRatio);
    const float lineHeight = m_layout.height * k_LineHeightRatio;

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, Color{20, 20, 40, 230});
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, Color::Yellow());

    rGraphics.DrawText("Select Production", m_layout.x + padding, m_layout.y + padding, headerFontSize, Color::Yellow());

    if (m_availableItems.empty())
    {
        rGraphics.DrawText("Nothing available to build", m_layout.x + padding, m_layout.y + lineHeight * 2.0f, entryFontSize, Color::White());
        return;
    }

    for (size_t i = 0; i < m_availableItems.size(); ++i)
    {
        const Rectangle_t& rect = m_entryRects[i];
        rGraphics.DrawText(m_availableItems[i]->GetName(), rect.x + padding, rect.y, entryFontSize, Color::White());
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
