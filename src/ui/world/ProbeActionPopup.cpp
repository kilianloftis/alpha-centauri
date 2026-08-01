#include "ui/world/ProbeActionPopup.h"
#include "graphics/Graphics.h"
#include "input/Input.h"
#include "ui/style/UiStyle.h"

namespace ac
{

ProbeActionPopup::ProbeActionPopup(
    WindowLayout_t layout,
    std::vector<std::pair<ProbeActionId_t, std::string>> actions,
    std::function<void(ProbeActionId_t actionId)> onActionSelected
)
    : UIElement(layout)
    , m_actions(std::move(actions))
    , m_onActionSelected(std::move(onActionSelected))
{
    CacheEntryRects_();
}

void ProbeActionPopup::CacheEntryRects_()
{
    const auto& style = Style().productionSelectorPopup;
    const float lineHeight = m_layout.height * style.lineHeightRatio;
    float offsetY = lineHeight * style.headerLineOffset;
    for (size_t i = 0; i < m_actions.size(); ++i)
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

void ProbeActionPopup::Render(Graphics& rGraphics)
{
    if (m_bShouldClose)
    {
        return;
    }

    const auto& style = Style().productionSelectorPopup;
    const float padding = style.paddingRatio * m_layout.width;
    const unsigned int headerFontSize =
        static_cast<unsigned int>(m_layout.height * style.headerFontSizeRatio);
    const unsigned int entryFontSize =
        static_cast<unsigned int>(m_layout.height * style.entryFontSizeRatio);

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height,
                             style.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.borderColor);

    rGraphics.DrawText("Probe Actions", m_layout.x + padding, m_layout.y + padding,
                       headerFontSize, style.headerColor);

    for (size_t i = 0; i < m_actions.size(); ++i)
    {
        const Rectangle_t& rect = m_entryRects[i];
        rGraphics.DrawText(m_actions[i].second, rect.x + padding, rect.y, entryFontSize,
                           style.entryColor);
    }
}

bool ProbeActionPopup::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
        return true;
    }
    return false;
}

void ProbeActionPopup::HandleMouseClick(const MouseEvent_t& rEvent)
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
            if (m_onActionSelected)
            {
                m_onActionSelected(m_actions[i].first);
            }
            m_bShouldClose = true;
            return;
        }
    }
}

} // namespace ac
