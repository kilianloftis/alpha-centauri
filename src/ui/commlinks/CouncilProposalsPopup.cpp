#include "ui/commlinks/CouncilProposalsPopup.h"
#include "game/council/CouncilProposalConfig.h"
#include "graphics/Graphics.h"
#include "input/Input.h"
#include "ui/style/UiStyle.h"

namespace ac
{

CouncilProposalsPopup::CouncilProposalsPopup(
    std::vector<const CouncilProposalConfig_t*> proposals,
    WindowLayout_t layout,
    std::function<void(const CouncilProposalConfig_t&)> onProposalSelected
)
    : UIElement(layout)
    , m_proposals(std::move(proposals))
    , m_onProposalSelected(std::move(onProposalSelected))
{
    CacheEntryRects_();
}

void CouncilProposalsPopup::CacheEntryRects_()
{
    const auto& style = Style().productionSelectorPopup;
    const float lineHeight = m_layout.height * style.lineHeightRatio;
    float offsetY = lineHeight * style.headerLineOffset;
    for (size_t i = 0; i < m_proposals.size(); ++i)
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

void CouncilProposalsPopup::Render(Graphics& rGraphics)
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
    const float lineHeight = m_layout.height * style.lineHeightRatio;

    rGraphics.DrawFilledRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height,
                             style.backgroundColor);
    rGraphics.DrawRect(m_layout.x, m_layout.y, m_layout.width, m_layout.height, style.borderColor);

    rGraphics.DrawText(
        "Planetary Council",
        m_layout.x + padding,
        m_layout.y + padding,
        headerFontSize,
        style.headerColor);

    if (m_proposals.empty())
    {
        rGraphics.DrawText(
            "No proposals available",
            m_layout.x + padding,
            m_layout.y + lineHeight * style.headerLineOffset,
            entryFontSize,
            style.hintColor);
        return;
    }

    for (size_t i = 0; i < m_proposals.size(); ++i)
    {
        const Rectangle_t& rect = m_entryRects[i];
        rGraphics.DrawText(
            m_proposals[i]->name,
            rect.x + padding,
            rect.y,
            entryFontSize,
            style.entryColor);
    }
}

bool CouncilProposalsPopup::HandleKey(const KeyEvent_t& rEvent)
{
    if (rEvent.key == Key_t::Escape)
    {
        m_bShouldClose = true;
        return true;
    }
    return false;
}

void CouncilProposalsPopup::HandleMouseClick(const MouseEvent_t& rEvent)
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
            if (m_onProposalSelected && m_proposals[i])
            {
                m_onProposalSelected(*m_proposals[i]);
                m_bShouldClose = true;
            }
            return;
        }
    }
}

} // namespace ac
