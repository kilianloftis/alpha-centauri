#include "ui/MultiSelectListPopup.h"

#include "graphics/Graphics.h"
#include "input/Input.h"
#include "ui/SpriteLibrary.h"
#include "ui/style/DrawPanelChrome.h"
#include "ui/style/UiStyle.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace ac
{

MultiSelectListPopup::MultiSelectListPopup(std::string title,
                                           std::string emptyMessage,
                                           std::vector<std::string> labels,
                                           size_t maxSelections,
                                           std::string confirmLabel,
                                           std::function<void(std::vector<size_t>)> onConfirm,
                                           WindowLayout_t layout,
                                           const ListSelectorPopupStyle_t& rStyle,
                                           SpriteLibrary* pSprites)
    : UIElement(layout)
    , m_title(std::move(title))
    , m_emptyMessage(std::move(emptyMessage))
    , m_labels(std::move(labels))
    , m_maxSelections(maxSelections)
    , m_confirmLabel(std::move(confirmLabel))
    , m_onConfirm(std::move(onConfirm))
    , m_rStyle(rStyle)
    , m_pSprites(pSprites)
{
    if (m_maxSelections == 0 || !m_onConfirm)
    {
        throw std::invalid_argument("MultiSelectListPopup '" + m_title
                                    + "' needs maxSelections and onConfirm");
    }
    CacheEntryRects_();
}

size_t MultiSelectListPopup::VisibleRowCount_() const
{
    const float lineHeight = LineHeight_();
    if (lineHeight <= 0.0f)
    {
        return 0;
    }

    constexpr size_t k_FooterRows = 2;
    const float contentTop = lineHeight * m_rStyle.headerLineOffset;
    const float footerHeight = lineHeight * static_cast<float>(k_FooterRows);
    const float contentHeight = m_layout.height - contentTop - footerHeight;
    if (contentHeight <= 0.0f)
    {
        return 0;
    }

    const size_t fits = static_cast<size_t>(contentHeight / lineHeight);
    if (m_labels.size() <= fits)
    {
        return fits;
    }
    return fits > 0 ? fits - 1 : 0;
}

void MultiSelectListPopup::CacheEntryRects_()
{
    const float lineHeight = LineHeight_();

    m_entryRects.clear();
    const size_t visible = std::min(VisibleRowCount_(), m_labels.size() - m_scrollOffset);
    float offsetY = lineHeight * m_rStyle.headerLineOffset;
    for (size_t i = 0; i < visible; ++i)
    {
        m_entryRects.push_back(Rectangle_t{m_layout.x, m_layout.y + offsetY, m_layout.width,
                                           lineHeight});
        offsetY += lineHeight;
    }

    const float footerY = m_layout.y + m_layout.height - lineHeight * 2.0f;
    m_confirmRect = Rectangle_t{m_layout.x, footerY, m_layout.width, lineHeight};
    m_cancelRect = Rectangle_t{m_layout.x, footerY + lineHeight, m_layout.width, lineHeight};
}

void MultiSelectListPopup::Render(Graphics& rGraphics)
{
    if (m_bShouldClose)
    {
        return;
    }
    const float padding = Padding_();
    const auto headerFontSize =
        static_cast<unsigned int>(m_layout.height * m_rStyle.headerFontSizeRatio);
    DrawPanelChrome(rGraphics, m_pSprites, m_layout, m_rStyle.backgroundColor,
                    m_rStyle.backgroundSprite, m_rStyle.borderColor, m_rStyle.borderWidth);
    rGraphics.DrawText(m_title, m_layout.x + padding, m_layout.y + padding, headerFontSize,
                       m_rStyle.headerColor);
    if (m_labels.empty())
    {
        rGraphics.DrawText(m_emptyMessage, m_layout.x + padding,
                           m_layout.y + LineHeight_() * m_rStyle.headerLineOffset,
                           EntryFontSize_(), m_rStyle.hintColor);
        DrawFooter_(rGraphics);
        return;
    }
    DrawEntries_(rGraphics);
    DrawScrollIndicator_(rGraphics);
    DrawFooter_(rGraphics);
}

float MultiSelectListPopup::Padding_() const
{
    return m_rStyle.paddingRatio * m_layout.width;
}

unsigned int MultiSelectListPopup::EntryFontSize_() const
{
    return static_cast<unsigned int>(m_layout.height * m_rStyle.entryFontSizeRatio);
}

float MultiSelectListPopup::LineHeight_() const
{
    return m_layout.height * m_rStyle.lineHeightRatio;
}

void MultiSelectListPopup::DrawEntries_(Graphics& rGraphics) const
{
    const float padding = Padding_();
    const unsigned int entryFontSize = EntryFontSize_();
    for (size_t i = 0; i < m_entryRects.size(); ++i)
    {
        const Rectangle_t& rRect = m_entryRects[i];
        const size_t choiceIndex = m_scrollOffset + i;
        std::string label = m_labels[choiceIndex];
        if (m_selected.contains(choiceIndex))
        {
            label = "* " + label;
        }
        rGraphics.DrawText(label, rRect.x + padding, rRect.y, entryFontSize, m_rStyle.entryColor);
    }
}

void MultiSelectListPopup::DrawFooter_(Graphics& rGraphics) const
{
    const float padding = Padding_();
    const unsigned int entryFontSize = EntryFontSize_();
    rGraphics.DrawText(m_confirmLabel, m_confirmRect.x + padding, m_confirmRect.y, entryFontSize,
                       m_rStyle.entryColor);
    rGraphics.DrawText("Cancel", m_cancelRect.x + padding, m_cancelRect.y, entryFontSize,
                       m_rStyle.entryColor);
}

void MultiSelectListPopup::DrawScrollIndicator_(Graphics& rGraphics) const
{
    const size_t shown = m_scrollOffset + m_entryRects.size();
    if (m_scrollOffset == 0 && shown >= m_labels.size())
    {
        return;
    }
    const std::string indicator = "[" + std::to_string(m_scrollOffset + 1) + "-"
                                  + std::to_string(shown) + " of "
                                  + std::to_string(m_labels.size()) + "]";
    rGraphics.DrawText(indicator, m_layout.x + Padding_(), m_confirmRect.y - LineHeight_(),
                       EntryFontSize_(), m_rStyle.hintColor);
}

void MultiSelectListPopup::ToggleSelection_(size_t choiceIndex)
{
    if (m_selected.contains(choiceIndex))
    {
        m_selected.erase(choiceIndex);
        return;
    }
    if (m_selected.size() >= m_maxSelections)
    {
        return;
    }
    m_selected.insert(choiceIndex);
}

void MultiSelectListPopup::Confirm_()
{
    if (m_selected.empty())
    {
        return;
    }
    std::vector<size_t> selected(m_selected.begin(), m_selected.end());
    std::sort(selected.begin(), selected.end());
    m_bShouldClose = true;
    m_onConfirm(std::move(selected));
}

bool MultiSelectListPopup::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
        return true;
    }

    const size_t visible = VisibleRowCount_();
    if (visible == 0 || m_labels.size() <= visible)
    {
        return false;
    }

    if (rEvent.key == Key_t::ArrowDown && m_scrollOffset + visible < m_labels.size())
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

void MultiSelectListPopup::HandleMouseClick(const MouseEvent_t& rEvent)
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

    if (ContainsMouseCoord(m_confirmRect, rEvent))
    {
        Confirm_();
        return;
    }
    if (ContainsMouseCoord(m_cancelRect, rEvent))
    {
        m_bShouldClose = true;
        return;
    }

    for (size_t i = 0; i < m_entryRects.size(); ++i)
    {
        if (!ContainsMouseCoord(m_entryRects[i], rEvent))
        {
            continue;
        }
        ToggleSelection_(m_scrollOffset + i);
        return;
    }
}

} // namespace ac
