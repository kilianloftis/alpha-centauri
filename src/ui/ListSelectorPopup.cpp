#include "ui/ListSelectorPopup.h"

#include "graphics/Graphics.h"
#include "input/Input.h"
#include "ui/style/UiStyle.h"

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ac
{

ListSelectorPopup::ListSelectorPopup(std::string title,
                                     std::string emptyMessage,
                                     std::vector<PopupChoice_t> choices,
                                     WindowLayout_t layout,
                                     const ListSelectorPopupStyle_t& rStyle)
    : UIElement(layout)
    , m_title(std::move(title))
    , m_emptyMessage(std::move(emptyMessage))
    , m_choices(std::move(choices))
    , m_rStyle(rStyle)
{
    for (const PopupChoice_t& rChoice : m_choices)
    {
        if (!rChoice.onChosen)
        {
            throw std::invalid_argument("ListSelectorPopup '" + m_title + "' choice '"
                                        + rChoice.label + "' has no action");
        }
    }
    CacheEntryRects_();
}

size_t ListSelectorPopup::VisibleRowCount_() const
{
    const float lineHeight = m_layout.height * m_rStyle.lineHeightRatio;
    if (lineHeight <= 0.0f)
    {
        return 0;
    }

    const float contentTop = lineHeight * m_rStyle.headerLineOffset;
    const float contentHeight = m_layout.height - contentTop;
    if (contentHeight <= 0.0f)
    {
        return 0;
    }

    const size_t fits = static_cast<size_t>(contentHeight / lineHeight);
    if (m_choices.size() <= fits)
    {
        return fits;
    }
    // The bottom line belongs to the overflow indicator when there is one; otherwise it would
    // be drawn over the last row, which would also still be clickable underneath it.
    return fits > 0 ? fits - 1 : 0;
}

void ListSelectorPopup::CacheEntryRects_()
{
    const float lineHeight = m_layout.height * m_rStyle.lineHeightRatio;

    m_entryRects.clear();
    const size_t visible = std::min(VisibleRowCount_(), m_choices.size() - m_scrollOffset);
    float offsetY = lineHeight * m_rStyle.headerLineOffset;
    for (size_t i = 0; i < visible; ++i)
    {
        m_entryRects.push_back(Rectangle_t{m_layout.x, m_layout.y + offsetY, m_layout.width,
                                           lineHeight});
        offsetY += lineHeight;
    }
}

void ListSelectorPopup::Render(Graphics& rGraphics)
{
    if (m_bShouldClose)
    {
        return;
    }

    const float padding = m_rStyle.paddingRatio * m_layout.width;
    const auto headerFontSize =
        static_cast<unsigned int>(m_layout.height * m_rStyle.headerFontSizeRatio);
    const auto entryFontSize =
        static_cast<unsigned int>(m_layout.height * m_rStyle.entryFontSizeRatio);
    const float lineHeight = m_layout.height * m_rStyle.lineHeightRatio;

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height,
                             m_rStyle.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height,
                       m_rStyle.borderColor, m_rStyle.borderWidth);
    rGraphics.DrawText(m_title, m_layout.x + padding, m_layout.y + padding, headerFontSize,
                       m_rStyle.headerColor);

    if (m_choices.empty())
    {
        rGraphics.DrawText(m_emptyMessage, m_layout.x + padding,
                           m_layout.y + lineHeight * m_rStyle.headerLineOffset, entryFontSize,
                           m_rStyle.hintColor);
        return;
    }

    for (size_t i = 0; i < m_entryRects.size(); ++i)
    {
        const Rectangle_t& rRect = m_entryRects[i];
        rGraphics.DrawText(m_choices[m_scrollOffset + i].label, rRect.x + padding, rRect.y,
                           entryFontSize, m_rStyle.entryColor);
    }

    // Named rather than dropped off the bottom edge, which is what an unbounded layout did.
    const size_t shown = m_scrollOffset + m_entryRects.size();
    if (m_scrollOffset > 0 || shown < m_choices.size())
    {
        const std::string indicator = "[" + std::to_string(m_scrollOffset + 1) + "-"
                                      + std::to_string(shown) + " of "
                                      + std::to_string(m_choices.size()) + "]";
        rGraphics.DrawText(indicator, m_layout.x + padding,
                           m_layout.y + m_layout.height - lineHeight, entryFontSize,
                           m_rStyle.hintColor);
    }
}

bool ListSelectorPopup::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
        return true;
    }

    const size_t visible = VisibleRowCount_();
    if (visible == 0 || m_choices.size() <= visible)
    {
        return false;
    }

    if (rEvent.key == Key_t::ArrowDown && m_scrollOffset + visible < m_choices.size())
    {
        ++m_scrollOffset;
        CacheEntryRects_();
        return true;
    }
    if (rEvent.key == Key_t::ArrowUp && m_scrollOffset > 0)
    {
        --m_scrollOffset;
        CacheEntryRects_();
        return true;
    }
    return false;
}

void ListSelectorPopup::HandleMouseClick(const MouseEvent_t& rEvent)
{
    if (m_bShouldClose || rEvent.button != MouseButton_t::Left)
    {
        return;
    }

    // Modal routing delivers presses from outside the popup too, so this is the dismiss path.
    if (!ContainsMouseCoord(m_layout, rEvent))
    {
        m_bShouldClose = true;
        return;
    }

    for (size_t i = 0; i < m_entryRects.size(); ++i)
    {
        if (ContainsMouseCoord(m_entryRects[i], rEvent))
        {
            // Close before the callback: a handler that resumes turn processing is gated on
            // this popup no longer counting as an open modal (IGameView::HasModalElement).
            m_bShouldClose = true;
            m_choices[m_scrollOffset + i].onChosen();
            return;
        }
    }
}

} // namespace ac
